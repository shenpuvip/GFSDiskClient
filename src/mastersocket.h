#ifndef MASTERSOCKET_H
#define MASTERSOCKET_H

#include "mtcpsocket.h"

#include <string>

class MasterSocket : public MTcpSocket
{
    Q_OBJECT

public:
    MasterSocket(const QString& masterHostname, uint16_t masterPort, QObject *parent = nullptr) : MTcpSocket(parent)
    {
        this->masterHostname = masterHostname;
        this->masterPort = masterPort;
    }

    void connectToMaster()
    {
        this->connectToHost(masterHostname, masterPort);
    }

    void asyncReady()
    {
        this->ASYNC();
        connect(this, &MasterSocket::onMessage, this, &MasterSocket::handleMessage);
    }

signals:
    void readyFileListRes(const std::string& message);
    void readyFileInfoRes(const std::string& message);

    void readyUpFileRes(const std::string& message);
    void readyUpFileBlockRes(const std::string& message);

    void readyAddFileRes(const std::string& message);
    void readyRMFileRes(const std::string& message);
    void readyMVFileRes(const std::string& message);

private:
    void handleMessage(msgtype_t msgtype, const std::string& message)
    {
        switch (msgtype)
        {
        case FILELIST_RES:
            emit this->readyFileListRes(message);
            break;
        case FILEINFO_RES:
            emit this->readyFileInfoRes(message);
            break;
        case UPFILE_RES:
            emit this->readyUpFileRes(message);
            break;
        case UPFILE_BLOCK_RES:
            emit this->readyUpFileBlockRes(message);
            break;
        case ADDFILE_RES:
            emit this->readyAddFileRes(message);
            break;
        case RMFILE_RES:
            emit this->readyRMFileRes(message);
            break;
        case MVFILE_RES:
            emit this->readyMVFileRes(message);
            break;
        default:
            break;
        }
    }

private:
    QString masterHostname;
    uint16_t masterPort;
};

#endif // MASTERSOCKET_H
