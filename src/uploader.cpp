#include "updowner.h"

UPDOWNER::UPDOWNER(const QString& host, uint16_t port, std::list<BLOCKTASKPtr> tasks)
{
    this->host = host;
    this->port = port;
    this->tasks = tasks;
}

UPDOWNER::~UPDOWNER()
{
    //qDebug() << "UPDOWNER delete";
}

void UPDOWNER::start()
{
    if(isStart) return;
    isStart = 1;
    socket.reset(new MTcpSocket);
    socket->connectToHost(host, port);
    //QObject::connect(socket.get(), &MTcpSocket::connected, this, &UPDOWNER::onConnected);
    this->socket->ASYNC();
    QObject::connect(this->socket.get(), &MTcpSocket::onMessage, this, &UPDOWNER::onMessage);
    QObject::connect(socket.get(), &MTcpSocket::bytesWritten, this, &UPDOWNER::onWriteComplete);
    QObject::connect(socket.get(), &MTcpSocket::disconnected, this, &UPDOWNER::onDisconnected);

    int reTry = 5;
    while(reTry-- )
    {
          if(this->socket->waitForConnected())
          {
              startTask();
              reTry = 100;
              break;
          }
    }

    if(reTry < 0)
    {
        QString error = "can't connect to chunkserver";
        emit readyError(error);
    }
}

void UPDOWNER::stop()
{
    socket->disconnectFromHost();
}

void UPDOWNER::startTask()
{
    if(tasks.empty())
    {
        emit this->readyFinshed();
        return;
    }
    task = tasks.front();
    tasks.pop_front();
    if(task->type == task->DOWN)
    {
        Json::FastWriter fw;
        Json::Value req;
        req["blockhash"] = task->blockhash;
        socket->send(DOWNBLOCK_REQ, fw.write(req));
    }
    else if(task->type == task->UP)
    {
        Json::FastWriter fw;
        Json::Value req;
        req["blockhash"] = task->blockhash;
        req["blocksize"] = Json::Int64(task->length);
        socket->send(UPBLOCK_REQ, fw.write(req));
    }
}

void UPDOWNER::onMessage(msgtype_t msgtype, const std::string &message)
{
    //qDebug() << msgtype << QString::fromStdString(message);
    switch (msgtype)
    {
    case UPBLOCK_RES:
    {
        Json::Reader rd;
        Json::Value msg;
        rd.parse(message, msg);
        if(msg.isMember("error"))
        {
            //error;
            QString error = QString::fromStdString(msg["error"].asString());
            emit task->readyError(error);
            startTask();
            return;
        }
        emit task->readyStart();
        onWriteComplete(0);
    }
        break;
    case UPBLOCK_OVER:
    {
        emit task->readyOver();
        startTask();
    }
        break;
    case DOWNBLOCK_REQ:
    {
        Json::Reader rd;
        Json::Value msg;
        rd.parse(message, msg);
        if(msg.isMember("error"))
        {
            //error;
            QString error = QString::fromStdString(msg["error"].asString());
            emit task->readyError(error);
            startTask();
            return;
        }
        emit task->readyStart();
    }
        break;
    case DOWNBLOCK_DATA:
    {
        if(task->fp.get())
        {
            if(task->offset != -1)
                task->fp->seek(task->offset);
            task->fp->write(message.data(), message.size());
            if(task->offset != -1)
                task->offset += message.size();
            emit task->readyDataSize(message.size());
        }
    }
        break;
    case DOWNBLOCK_OVER:
    {
        task->finshed = 1;
        emit task->readyOver();
        startTask();
    }
        break;
    default:
        break;
    }
}

void UPDOWNER::onWriteComplete(qint64 bytes)
{
    if(bytes < 0) return;
    if(task->type == task->UP && task->finshed==0)
    {
        char buf[1024 * 1024];
        int bufsz = sizeof(buf);
        int len = (task->length == -1) ? bufsz : (bufsz < task->length ? bufsz : task->length);
        if(task->offset != -1)
            task->fp->seek(task->offset);
        int nread = task->fp->read(buf, len);
        (task->offset != -1) ? task->offset += nread : 0;
        (task->length != -1) ? task->length -= nread : 0;
        if(nread > 0)
        {
            this->socket->send(UPBLOCK_DATA, buf, nread);
            emit task->readyDataSize(nread);
        }
        else
        {
            this->socket->send(UPBLOCK_OVER);
            task->finshed = 1;
        }
    }
}

void UPDOWNER::onDisconnected()
{
    ;
}
