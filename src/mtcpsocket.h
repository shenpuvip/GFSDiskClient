#ifndef MTCPSOCKET_H
#define MTCPSOCKET_H

#include "msgcode.h"
#include <memory>
#include <string>

#include <QTcpSocket>
#include <QByteArray>
#include <QtEndian>
#include <QDebug>

class MTcpSocket : public QTcpSocket
{
    Q_OBJECT

public:
    MTcpSocket(QObject *parent = nullptr) : QTcpSocket(parent)
    {
        ;
    }

    void ASYNC()
    {
        connect(this, &QTcpSocket::readyRead, this, &MTcpSocket::onReadyRead);
    }

    void send(msgtype_t msgtype)
    {
        QByteArray data;
        msgh_t len = FLEN + MSGTYPELEN;
        msgflag_t flag = FLAG;
        len = qToBigEndian(len);
        flag = qToBigEndian(flag);
        msgtype = qToBigEndian(msgtype);
        data.append((char*)&len, sizeof(len));
        data.append((char*)&flag, sizeof(flag));
        data.append((char*)&msgtype, sizeof(msgtype));
        this->write(data);
    }

    void send(msgtype_t msgtype, const std::string& message)
    {
        QByteArray data;
        msgh_t len = FLEN + MSGTYPELEN + message.size();
        msgflag_t flag = FLAG;
        len = qToBigEndian(len);
        flag = qToBigEndian(flag);
        msgtype = qToBigEndian(msgtype);
        data.append((char*)&len, sizeof(len));
        data.append((char*)&flag, sizeof(flag));
        data.append((char*)&msgtype, sizeof(msgtype));
        data.append(message.data(), message.size());
        this->write(data);
    }

    void send(msgtype_t msgtype, const char* buf, int buflen)
    {
        QByteArray data;
        msgh_t len = FLEN + MSGTYPELEN + buflen;
        msgflag_t flag = FLAG;
        len = qToBigEndian(len);
        flag = qToBigEndian(flag);
        msgtype = qToBigEndian(msgtype);
        data.append((char*)&len, sizeof(len));
        data.append((char*)&flag, sizeof(flag));
        data.append((char*)&msgtype, sizeof(msgtype));
        data.append(buf, buflen);
        this->write(data);
    }

    bool getOneMessage(msgtype_t& msgtype, std::string& message)
    {
        msgh_t len = -1;
        msgflag_t flag;
        int needLen = HLEN;
        while(true)
        {
            if(this->bytesAvailable() < needLen)
                this->waitForReadyRead();

            if(len < 0 && this->bytesAvailable() >= HLEN )
            {
                this->peekNumber(len);
                if(len < MINHLEN || len > MAXHLEN)
                {
                    qDebug() << "SOCKET ERROR";
                    return false;
                }
                needLen = HLEN + len;
            }

            if(len >= 0 && this->bytesAvailable() >= needLen)
            {
                this->read(HLEN);
                this->readNumber(flag);
                this->readNumber(msgtype);
                len -= FLEN + MSGTYPELEN;
                message.assign(this->read(len).toStdString());
                return true;
            }
        }
        return false;
    }

signals:
    void onMessage(msgtype_t msgtype, const std::string& message);

private slots:
    void onReadyRead()
    {
        while(this->bytesAvailable() >= HLEN)
        {
            msgh_t len;
            this->peekNumber(len);
            if(len < MINHLEN || len > MAXHLEN)
            {
                this->disconnectFromHost();
                break;
            }
            else if(this->bytesAvailable() >= len + HLEN)
            {
                this->read(HLEN);
                msgflag_t flag;
                msgtype_t msgtype;
                this->readNumber(flag);
                this->readNumber(msgtype);
                if(flag != FLAG)
                {
                    this->disconnectFromHost();
                    break;
                }
                len -= FLEN + MSGTYPELEN;
                std::string message = this->read(len).toStdString();;
                emit this->onMessage(msgtype, message);
            }
            else
            {
                break;
            }
        }
    }

private:

    template<typename T>
    inline void peekNumber(T& x)
    {
        this->peek((char*)&x, sizeof(x));
        x = qFromBigEndian(x);
    }

    template<typename T>
    inline void readNumber(T& x)
    {
        this->read((char*)&x, sizeof(x));
        x = qFromBigEndian(x);
    }
};

#endif // MTCPSOCKET_H
