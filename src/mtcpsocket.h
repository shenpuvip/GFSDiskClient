#ifndef MTCPSOCKET_H
#define MTCPSOCKET_H

#include <memory>
#include "msgcode.h"

#include <QTcpSocket>
#include <QByteArray>
#include <QDataStream>

class MTcpSocket : public QTcpSocket
{
    Q_OBJECT

public:
    MTcpSocket()
    {
        ;
    }

    void CONNECTREADY()
    {
        connect(this, &QTcpSocket::readyRead, this, &MTcpSocket::onReadyRead);
    }

    void send(msgtype_t msgtype)
    {
        QByteArray data;
        QDataStream buf(&data, QIODevice::WriteOnly);
        buf.setByteOrder(QDataStream::BigEndian);
        msgh_t len = FLEN + MSGTYPELEN;
        buf << len;
        buf << FLAG;
        buf << msgtype;
        this->write(data);
    }

    void send(msgtype_t msgtype, std::string message)
    {
        QByteArray data;
        QDataStream buf(&data, QIODevice::WriteOnly);
        buf.setByteOrder(QDataStream::BigEndian);
        msgh_t len = FLEN + MSGTYPELEN + message.size();
        buf << len;
        buf << FLAG;
        buf << msgtype;
        buf.writeRawData(message.data(), message.size());
        this->write(data);
    }

    bool getOneMessage(msgtype_t& msgtype, std::string& message)
    {
        QDataStream out(&buffer_, QIODevice::ReadOnly);
        out.setByteOrder(QDataStream::BigEndian);
        msgh_t len = -1;
        msgflag_t flag;
        int buflen = buffer_.size();
        int needLen = HLEN;
        while(true)
        {
            if(buflen < needLen)
            {
                if(this->bytesAvailable() <= 0)
                    this->waitForReadyRead();
                buffer_.append(this->readAll());
                buflen = buffer_.size();
            }

            if(len < 0 && buflen >= HLEN )
            {
                out >> len;
                if(len < MINHLEN || len > MAXHLEN)
                {
                    qDebug() << "SOCKET ERROR";
                    return false;
                }
                needLen = HLEN + len;
            }

            if(len >= 0 && buflen >= needLen)
            {
                out >> flag >> msgtype;
                len -= FLEN + MSGTYPELEN;
                std::unique_ptr<char[]> tmp(new char[len]);
                out.readRawData(tmp.get(), len);
                message.assign(tmp.get(), len);
                buffer_.remove(0, needLen);
                return true;
            }
        }
        return false;
    }

signals:
    void onMessage(msgtype_t msgtype, std::string message);

private slots:
    void onReadyRead()
    {
        buffer_.append(this->readAll());
        int buflen = buffer_.size();
        while(buflen >= HLEN)
        {
            QDataStream out(&buffer_, QIODevice::ReadOnly);
            msgh_t len;
            out >> len;
            if(len < MINHLEN || len > MAXHLEN)
            {
                this->disconnectFromHost();
                break;
            }
            else if(buflen >= len + HLEN)
            {
                msgflag_t flag;
                msgtype_t msgtype;
                out >> flag >> msgtype;
                if(flag != FLAG)
                {
                    this->disconnectFromHost();
                    break;
                }
                len -= FLEN + MSGTYPELEN;
                std::unique_ptr<char[]> tmp(new char[len+1]);
                out.readRawData(tmp.get(), len);
                std::string message(tmp.get(), len);
                emit onMessage(msgtype, message);
                buffer_.remove(0, len+HLEN+FLEN+MSGTYPELEN);
                buflen = buffer_.size();
            }
            else
            {
                break;
            }
        }
    }

private:
    QByteArray buffer_;

};

#endif // MTCPSOCKET_H
