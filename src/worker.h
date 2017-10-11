#ifndef WORKER_H
#define WORKER_H

#include <memory>
#include "mtcpsocket.h"

#include <QThread>
#include <QTcpSocket>
#include <QFile>

struct TASKRESULT
{
    int statuscode;
    QString status;
    int nprogress;
};

struct TASK
{
    enum {DOWN = 1, UP = 2};
    int taskType;
    QString localfilepath;
    QString remotefilepath;
};

class WORKER : public QObject
{
    Q_OBJECT

public:
    WORKER(const QString& masterhost, uint16_t masterport, TASK task);
    ~WORKER();

    void doWork();
    void doUpFile(TASK& task);
    void doDownFile(TASK& task);

    static QString calcHash(QFile& f, qint64 len);

signals:
    void startWork();
    void deleteSelf();
    void resultReady(const TASKRESULT& result);
    void finshed(const TASK& task);

private:
    TASK task;
    QString masterhost;
    uint16_t masterport;
    std::shared_ptr<MTcpSocket> mastersocket;
};

#endif // WORKER_H
