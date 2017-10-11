#include "worker.h"

#include "msgcode.h"
#include <json/json.h>

#include <QByteArray>
#include <QDataStream>

#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDebug>


WORKER::WORKER(const QString &masterhost, uint16_t masterport, TASK task)
    :masterhost(masterhost), masterport(masterport)
{
    this->task = task;
    connect(this, &WORKER::deleteSelf, this, &WORKER::deleteLater);
    connect(this, &WORKER::startWork, this, &WORKER::doWork);
}

WORKER::~WORKER()
{
    qDebug() << "WORKER delete: " << this;
}

void WORKER::doWork()
{
    if(task.taskType == TASK::DOWN)
        doDownFile(task);
    else if(task.taskType == TASK::UP)
        doUpFile(task);
    emit this->deleteSelf();
}

void WORKER::doUpFile(TASK &task)
{
    TASKRESULT result;

    result.status = "calc filehash";
    result.nprogress = 0;
    emit resultReady(result);

    QFile f(task.localfilepath);
    f.open(QFile::ReadOnly);
    qint64 filesize = f.size();
    QString filehash = calcHash(f, filesize);

    result.status = "filehash calc over";
    emit resultReady(result);

    mastersocket.reset(new MTcpSocket);
    mastersocket->connectToHost(masterhost, masterport);
    mastersocket->waitForConnected();
    mastersocket->send(AUTH_REQ);

    result.status = "connect to master";
    emit resultReady(result);

    Json::Reader rd;
    Json::FastWriter fw;
    Json::Value req, msg;
    req["filename"] = task.remotefilepath.toStdString();
    req["filesize"] = Json::Int64(filesize);
    req["filehash"] = filehash.toStdString();
    mastersocket->send(UPFILE_REQ, fw.write(req));
    msgtype_t msgtype;
    std::string message;
    mastersocket->getOneMessage(msgtype, message);
    rd.parse(message, msg);
    if(msg.isMember("error"))
    {
        result.status = QString::fromStdString(msg["error"].asString());
        result.nprogress = 0;
        emit resultReady(result);
        f.close();
        return;
    }
    if(msg.isMember("fastpass") && msg["fastpass"].asInt())
    {
        result.status = "fastpass finshed";
        result.nprogress = 100;
        emit resultReady(result);
        f.close();
        return;
    }

    result.status = "calc file block hash";
    result.nprogress = 0;
    emit resultReady(result);

    qint64 blockSize = msg["blocksize"].asInt64();
    int n = (filesize + blockSize - 1) / blockSize;
    req["blocks"].resize(0);
    for(int i = 0; i < n; i ++)
    {
        Json::Value block;
        qint64 blocksize = (i!=n-1) ? blockSize : filesize-(n-1)*blockSize;
        block["blocksize"] = blocksize;
        f.seek(i*blockSize);
        block["blockhash"] = calcHash(f, blocksize).toStdString();
        req["blocks"].append(block);
    }
    qDebug() << QString::fromStdString(fw.write(req));
    mastersocket->send(UPFILE_BLOCK_REQ, fw.write(req));
    mastersocket->getOneMessage(msgtype, message);
    rd.parse(message, msg);
    if(msg.isMember("error"))
    {
        //error
        f.close();
        return;
    }


    result.status = "start upfile";
    result.nprogress = 0;
    emit resultReady(result);

    int order = 0;
    for(auto& block : msg["blocks"])
    {
        if(block.isMember("chunkip"))
        {
            //执行上传任务
            std::shared_ptr<MTcpSocket> upsocket(new MTcpSocket);
            std::string chunkip = block["chunkip"].asString();
            uint16_t chunkport = block["chunkport"].asUInt();
            upsocket->connectToHost(QString::fromStdString(chunkip), chunkport);
            upsocket->waitForConnected();
            Json::Value upreq;
            upreq["blockhash"] = block["blockhash"];
            upreq["blocksize"] = block["blocksize"];
            upsocket->send(UPBLOCK_REQ, fw.write(upreq));
            msgtype_t upmsgtype;
            std::string upmessage;
            upsocket->getOneMessage(upmsgtype, upmessage);
            Json::Value upres;
            rd.parse(upmessage, upres);
            if(upres.isMember("error"))
            {
                //error
                f.close();
                return;
            }
            f.seek(order * blockSize);
            qint64 tsz = 64 * 1024;
            qint64 nowsz = 0;
            qint64 blocksize = block["blocksize"].asInt64();
            while(nowsz < blocksize)
            {
                qint64 x = qMin(tsz, blocksize-nowsz);
                nowsz += x;
                upsocket->send(UPBLOCK_DATA, f.read(x).toStdString());
            }
            upsocket->send(UPBLOCK_OVER);
            upsocket->getOneMessage(upmsgtype, upmessage);
            if(upmsgtype == UPBLOCK_OVER)
            {
                //当前块上传完成
                result.status = "upfileing";
                result.nprogress = 100 * (order+1) / n;
                emit resultReady(result);
            }
            else
            {
                //TODO error
            }
            upsocket->disconnectFromHost();
        }
        ++order;
    }

    mastersocket->send(ADDFILE_REQ, fw.write(req));
    mastersocket->getOneMessage(msgtype, message);

    //全部上传完成
    result.status = "upfile finshed";
    result.nprogress = 100;
    emit resultReady(result);
    f.close();
    return ;
}

void WORKER::doDownFile(TASK& task)
{
    TASKRESULT result;
    mastersocket.reset(new MTcpSocket);
    mastersocket->connectToHost(masterhost, masterport);

    result.status = "wait connect master";
    result.nprogress = 0;
    emit resultReady(result);

    mastersocket->waitForConnected();
    mastersocket->send(AUTH_REQ);

    result.status = "wait get fileinfo";
    result.nprogress = 0;
    emit resultReady(result);

    Json::Reader rd;
    Json::FastWriter fw;
    Json::Value req;
    req["filename"] = task.remotefilepath.toStdString();
    mastersocket->send(FILEINFO_REQ, fw.write(req));
    msgtype_t msgtype;
    std::string message;
    Json::Value msg;
    mastersocket->getOneMessage(msgtype, message);
    rd.parse(message, msg);
    if(msg.isMember("error"))
    {
        //获取文件信息失败
        result.status = QString("get file info error:") + QString::fromStdString(msg["error"].asString());
        result.nprogress = 0;
        emit resultReady(result);
        return;
    }

    result.status = "start downfile";
    result.nprogress = 0;
    emit resultReady(result);

    QFile f(task.localfilepath);
    f.open(QFile::WriteOnly);
    int order = 1;
    int n = msg["blocks"].size();
    for(auto& block : msg["blocks"])
    {
        Json::Value chunk = block["chunklist"];
        if(chunk.size() <= 0)
        {
            //本块无chunk可以下载
            result.status = "no chunklist";
            result.nprogress = 0;
            emit resultReady(result);
            f.close();
            return ;
        }

        result.status = "connect to chunkserver";
        emit resultReady(result);

        QString chunkip = QString::fromStdString(chunk[0]["chunkip"].asString());
        uint16_t chunkport = chunk[0]["chunkport"].asUInt();
        std::shared_ptr<MTcpSocket> downsocket(new MTcpSocket);
        qDebug() << chunkip << ":" << chunkport;
        downsocket->connectToHost(chunkip, chunkport);
        downsocket->waitForConnected();

        result.status = "connect chunkserver success";
        emit resultReady(result);

        Json::Value downreq;
        downreq["blockhash"] = block["blockhash"];
        downsocket->send(DOWNBLOCK_REQ, fw.write(downreq));
        downsocket->waitForBytesWritten();
        msgtype_t downmsgtype;
        std::string downmessage;
        Json::Value downmsg;
        downsocket->getOneMessage(downmsgtype, downmessage);
        qDebug() << int(downmsgtype) << QString::fromStdString(downmessage);
        rd.parse(downmessage, downmsg);
        if(msg.isMember("error"))
        {
            //当前块出错
            result.status = "down block error";
            emit resultReady(result);
            f.close();
            return;
        }

        result.status = "start Down Block:" + QString::number(order);
        emit resultReady(result);

        while(downsocket->getOneMessage(downmsgtype, downmessage), downmsgtype != DOWNBLOCK_OVER)
        {
            //qDebug() << "msgtype: " << int(downmsgtype) << QString::fromStdString(downmessage);
            qDebug() << "get data" << downmessage.size();
            f.write(downmessage.data(), downmessage.size());
            //break;
        }
        //当前块下载完成
        downsocket->disconnectFromHost();
        result.status = "downblocking";
        result.nprogress = 100 * order / n;
        emit resultReady(result);

        ++order;
    }

    //文件下载完成
    //f.rename(task.localfilepath);

    result.status = "downfile finshed";
    result.nprogress = 100;
    emit resultReady(result);

    f.close();
    emit this->finshed(task);
    return;
}

QString WORKER::calcHash(QFile& f, qint64 len)
{
    QCryptographicHash hash(QCryptographicHash::Md5);
    auto fsz = len;
    decltype(fsz) tsz = 1024 * 1024;
    while(fsz > 0)
    {
        hash.addData(f.read(qMin(fsz, tsz)));
        fsz -= qMin(fsz, tsz);
    }
    return QString(hash.result().toHex());
}
