#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <memory>

#include <QFile>
#include <QObject>
#include <QDebug>
#include <QProgressBar>

#include <QTime>

class TASKProgressBar : public QProgressBar
{
    Q_OBJECT

public:
    enum TYPE{DOWN, UP, CALCFILEHASH, CALCBLOCKHASH};

public:
    int64_t tatol;
    int64_t now;
    int statuscode;
    QString status;
    TYPE type;
    QString localfilepath;
    QString remotefilepath;
    std::shared_ptr<QFile> fptr;

    QTime lastTime;
    int64_t lastSize;
    double speed;

public:
    TASKProgressBar(int64_t tatol, const QString& localfilepath, const QString& remotefilepath, TYPE type = DOWN, QWidget* parent=0): QProgressBar(parent)
    {
        this->type = type;
        this->tatol = tatol;
        now = 0;

        this->localfilepath = localfilepath;
        this->remotefilepath = remotefilepath;

        this->setValue(0);
        this->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        connect(this, &TASKProgressBar::startTask, this, &TASKProgressBar::start);
    }

    ~TASKProgressBar()
    {
        QString message;
        if(type == UP)
            message = " UP";
        else if(type == DOWN)
            message = " DOWN";
        else if(type == CALCFILEHASH)
            message = " CALCFILEHASH";
        else if(type == CALCBLOCKHASH)
            message = " CALCMBLOCKHASH";
        qDebug() << "TASK delete" << message;
    }

    void start()
    {
        lastSize = 0;
        lastTime.start();
        speed = 0;
    }

    void handleData(int64_t sz)
    {
        now += sz;
        int value = now*100/tatol;
        this->setValue(value);

        QString taskType;
        if(type == DOWN)
            taskType = "下载";
        else if(type == UP)
            taskType = "上传";
        else if(type == CALCFILEHASH)
            taskType = "计算文件HASH";
        else if(type == CALCBLOCKHASH)
            taskType = "计算块HASH";

        QString filename = localfilepath.split('/').back();

        int msc = lastTime.elapsed();
        if(msc >= 100)
        {
            speed = 1.0*(now - lastSize)/(1024*1024);
            speed = speed / msc * 1000;
            lastSize = now;
            lastTime.restart();
        }

        QString SPEED = "0.0MB/s";
        if(speed < 1)
            SPEED = QString("%1 KB/s").arg(QString::number(speed*1024, 'f', 1));
        else
            SPEED = QString("%1 MB/s").arg(QString::number(speed, 'f', 1));

        //if(type == DOWN) SPEED = "";

        QString format = QString("%1 %2 %3 %4").arg(taskType).arg(filename)
                .arg("%p%").arg(SPEED);

        this->setFormat(format);
        if(now >= tatol)
        {
            emit readyTaskOver();
            emit finshed(this->remotefilepath);
        }
    }

    void setTaskOver(QString message)
    {
        this->setValue( this->maximum() );
        this->setFormat(message);
    }

signals:
    void finshed(QString remotefilepath);
    void readyTaskOver();
    void startTask();
};

#endif // TASK_H
