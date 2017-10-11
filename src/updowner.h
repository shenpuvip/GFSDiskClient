#ifndef UPDOWNER_H
#define UPDOWNER_H

#include "mtcpsocket.h"
#include <memory>
#include <stdio.h>
#include <list>

#include <json/json.h>

#include <QObject>
#include <QFile>
#include <QDebug>

typedef std::shared_ptr<QFile> FilePtr;

struct BLOCKTASK : public QObject
{
    Q_OBJECT
public:
    enum TYPE{DOWN, UP};
    TYPE type;
    std::string blockhash;
    FilePtr fp;
    int64_t offset = -1;
    int64_t length = -1;
    bool finshed = 0;

    BLOCKTASK(const std::string& blockhash, FilePtr fp, int64_t offset=-1, int64_t length = -1, TYPE type = DOWN)
        :blockhash(blockhash)
    {
        this->fp = fp;
        this->offset = offset;
        this->length = length;
        this->type = type;
    }

    ~BLOCKTASK()
    {
        //qDebug() << "BLOCKTASK delete";
    }

signals:
    void readyError(QString error);
    void readyStart();
    void readyDataSize(int64_t sz);
    void readyOver();
};
typedef std::shared_ptr<BLOCKTASK> BLOCKTASKPtr;

class UPDOWNER : public QObject
{
    Q_OBJECT

private:
    QString host;
    uint16_t port;
    std::shared_ptr<MTcpSocket> socket;
    bool isStart = 0;

    std::list<BLOCKTASKPtr> tasks;
    BLOCKTASKPtr task;

public:
    UPDOWNER(const QString& host, uint16_t port, std::list<BLOCKTASKPtr> tasks);
    ~UPDOWNER();

    void start();
    void stop();

private:
    void onConnected();
    void onMessage(msgtype_t msgtype, const std::string& message);
    void onWriteComplete(qint64 bytes);
    void onDisconnected();

    void startTask();

signals:
    void readyError(QString error);
    void readyFinshed();
};

#endif // UPDOWNER_H
