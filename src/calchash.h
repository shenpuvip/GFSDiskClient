#ifndef CALCHASH_H
#define CALCHASH_H

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <json/json.h>

class CalcHashTask : public QObject
{
    Q_OBJECT

public:
    enum TYPE{FILEHASH, BLOCKHASH};

private:
    QString filepath;
    TYPE type;
    int64_t BLOCKSIZE;
    QString remotefilename;

public:

    CalcHashTask(const QString& localfilepath,  const QString& remotefilename,
                 TYPE type = FILEHASH,
                 int64_t BLOCKSIZE = 64*1024*1024, QObject* parent=0):QObject(parent)
    {
        this->type = type;
        this->filepath = localfilepath;
        this->BLOCKSIZE = BLOCKSIZE;
        this->remotefilename = remotefilename;
    }

    void calcHash()
    {
        QFile f(filepath);
        f.open(QFile::ReadOnly);
        Json::Value result;
        result["localfilepath"] = filepath.toStdString();
        if(!f.isOpen())
        {
            result["error"] = "file can't open";
            emit readyOver(result);
            emit finshed();
            //deleteLater();
            return;
        }
        emit readyStart();
        result["filename"] = remotefilename.toStdString();
        result["filesize"] = f.size();
        if(type == FILEHASH)
        {
            QCryptographicHash hash(QCryptographicHash::Md5);
            while(!f.atEnd())
            {
                int64_t sz = 1*1024*1024;
                hash.addData(f.read(sz));
                emit readyProgress(sz);
            }
            result["filehash"] = hash.result().toHex().toStdString();
            emit readyOver(result);
            emit finshed();
            //deleteLater();
            return;
        }
        else if(type == BLOCKHASH)
        {
            result["blocks"].resize(0);
            int64_t filesize = f.size();
            int blockNumber = (filesize + BLOCKSIZE-1) / BLOCKSIZE;
            for(int i = 1; i <= blockNumber; i ++)
            {
                int64_t blocksize = (i!=blockNumber) ? BLOCKSIZE : filesize - (blockNumber-1)*BLOCKSIZE;
                Json::Value block;
                block["blocksize"] = Json::Int64(blocksize);
                QCryptographicHash hash(QCryptographicHash::Md5);
                int64_t fsz = blocksize;
                while(fsz > 0)
                {
                    int64_t tsz = 1 * 1024 * 1024;
                    tsz = qMin(fsz, tsz);
                    hash.addData(f.read(tsz));
                    fsz -= tsz;
                    emit readyProgress(tsz);
                }
                block["blockhash"] = hash.result().toHex().toStdString();
                result["blocks"].append(block);
            }
            emit readyOver(result);
            emit finshed();
            //deleteLater();
            return;
        }
    }

signals:
    void readyStart();
    void readyProgress(int64_t value);
    void readyOver(Json::Value hashinfo);
    void finshed();
};

#endif // CALCHASH_H
