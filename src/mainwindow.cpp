#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mtcpsocket.h"
#include "updowner.h"
#include "calchash.h"

#include <string>
#include <tuple>
#include <list>
#include <map>
#include <json/json.h>

#include <QMenu>
#include <QAction>
#include <QFile>
#include <QFileInfo>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressBar>

#include <QSound>

#include <QDebug>

MainWindow::MainWindow(QString masterHost, uint16_t masterPort,
                       MasterSocket* mastersocket, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    workThread.start();
    hashThread.start();

    this->masterHost = masterHost;
    this->masterPort = masterPort;
    if(mastersocket == nullptr)
    {
        this->master.reset(new MasterSocket(masterHost, masterPort));
        this->master->connectToMaster();
        this->master->asyncReady();
        this->master->waitForConnected();
        this->master->send(AUTH_REQ);
    }
    else
    {
        this->master.reset(mastersocket);
        this->master->asyncReady();
        this->master->waitForConnected();
        this->master->send(AUTH_REQ);
    }

    //关联信号与槽函数
    connect(this->master.get(), &MasterSocket::readyFileListRes, this, &MainWindow::refreshFileList);
    connect(this->master.get(), &MasterSocket::readyFileInfoRes, this, &MainWindow::downfileMessage);
    connect(this->master.get(), &MasterSocket::readyUpFileRes, this, &MainWindow::upfileMessage);
    connect(this->master.get(), &MasterSocket::readyUpFileBlockRes, this, &MainWindow::upfileBlockMessage);
    connect(this->master.get(), &MasterSocket::readyAddFileRes, this, &MainWindow::addFileRes);

    connect(this->master.get(), &MasterSocket::readyRMFileRes, [this](const std::string& message)
    {
        Json::Reader rd;
        Json::Value msg;
        rd.parse(message, msg);
        if(msg.isMember("error"))
        {
            QMessageBox::warning(this, "删除文件", "删除文件失败");
        }
        else
        {
            QMessageBox::warning(this, "删除文件成功", "删除文件成功");
        }
        this->master->send(FILELIST_REQ);
    });
    connect(this->master.get(), &MasterSocket::readyMVFileRes, [this](const std::string& message)
    {
        Json::Reader rd;
        Json::Value msg;
        rd.parse(message, msg);
        if(msg.isMember("error"))
        {
            QMessageBox::warning(this, "重命名文件", "重命名文件失败");
        }
        else
        {
            QMessageBox::warning(this, "重命名文件", "重命名文件成功");
        }
        this->master->send(FILELIST_REQ);
    });

    this->master->send(FILELIST_REQ);
}

MainWindow::~MainWindow()
{
    delete ui;
    workThread.quit();
    workThread.wait();

    hashThread.quit();
    hashThread.wait();
}

void MainWindow::on_remoteFileListWidget_customContextMenuRequested(const QPoint &pos)
{
    QListWidgetItem* curItem = ui->remoteFileListWidget->itemAt(pos);
    if(curItem == nullptr)
    {
        //显示刷新操作
        QMenu *popMenu = new QMenu( this );
        QAction *refresh = new QAction(tr("刷新"), popMenu);
        popMenu->addAction(refresh);

        connect(refresh, &QAction::triggered, [this]()
        {
            this->master->send(FILELIST_REQ);
        });

        popMenu->exec(QCursor::pos());
        delete popMenu;
    }
    else
    {
        //显示下载删除重命名文件等操作
        QMenu *popMenu = new QMenu( this );
        QAction *downfile = new QAction(tr("下载"), popMenu);
        popMenu->addAction(downfile);
        connect(downfile, &QAction::triggered, [&curItem, this]()
        {
            Json::FastWriter fw;
            Json::Value req;
            req["filename"] = curItem->text().toStdString();
            this->master->send(FILEINFO_REQ, fw.write(req));
        });

        QAction *deletefile = new QAction(tr("删除"), popMenu);
        popMenu->addAction(deletefile);
        connect(deletefile, &QAction::triggered, [this, &curItem]()
        {
            auto ret = QMessageBox::warning(this, "删除文件", "确认删除文件?",
                                            QMessageBox::Yes | QMessageBox::No);
            if(ret == QMessageBox::Yes)
            {
                Json::FastWriter fw;
                Json::Value req;
                req["filename"] = curItem->text().toStdString();
                this->master->send(RMFILE_REQ, fw.write(req));
            }
        });

        QAction *renamefile = new QAction(tr("重命名"), popMenu);
        popMenu->addAction(renamefile);
        connect(renamefile, &QAction::triggered, [this, &curItem]()
        {
            bool ok = false;
            QString newfilename = QInputDialog::getText(this, "重命名文件", "请输入新文件名:", QLineEdit::Normal, curItem->text(), &ok);
            if(ok)
            {
                Json::FastWriter fw;
                Json::Value req;
                req["oldfilename"] = curItem->text().toStdString();
                req["newfilename"] = newfilename.toStdString();
                this->master->send(MVFILE_REQ, fw.write(req));
            }
        });

        popMenu->exec(QCursor::pos());
        delete popMenu;
    }
}

void MainWindow::on_selectFileBtn_clicked()
{
    QString filepath = QFileDialog::getOpenFileName(nullptr, "选择要上传的文件");
    if(!filepath.isEmpty())
    {
        ui->localFilepathEdit->setText(filepath);
        ui->remoteFilepathEdit->setText(filepath.split('/').back());
    }
}

void MainWindow::refreshFileList(const std::string &message)
{
    Json::Reader rd;
    Json::Value msg;
    if(!rd.parse(message, msg)) return;
    ui->remoteFileListWidget->clear();
    for(auto& file : msg)
    {
        QString filename = QString::fromStdString(file["filename"].asString());
        int64_t filesize = file["filesize"].asInt64();

        QListWidgetItem *item = new QListWidgetItem(ui->remoteFileListWidget);
        ui->remoteFileListWidget->addItem(item);
        item->setText(filename);
        item->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
        QString filesizeString;
        if(filesize > 1*1024*1024)
            filesizeString = QString::number(filesize/(1024*1024)) + "MB";
        else
            filesizeString = QString::number(filesize/(1024)) + "KB";
        item->setToolTip(filename + "\nsize:" + filesizeString);
        item->setSizeHint(QSize(100, 100));
    }
}

void MainWindow::downfileMessage(const std::string& message)
{
    Json::Reader rd;
    Json::Value msg;
    if(!rd.parse(message, msg)) return;
    if(msg.isMember("error"))
    {
        QMessageBox::warning(this, "下载错误", "error" + QString::fromStdString(msg["error"].asString()));
        return;
    }

    Json::Value blocks = msg["blocks"];
    int error = 0;
    for(auto& block : blocks)
    {
        Json::Value chunk = block["chunklist"];
        if(chunk.size() <= 0)
        {
            //本块无chunk可以下载
            error = 1;
            break;
        }
        block["chunkip"] = chunk[0]["chunkip"];
        block["chunkport"] = chunk[0]["chunkport"];
    }
    if(error)
    {
        QMessageBox::warning(this, "无法下载", "存在块无服务可以下载");
        return;
    }
    QString remotefilepath = QString::fromStdString(msg["filename"].asString());
    QString localfilepath = QFileDialog::getSaveFileName(nullptr, "", remotefilepath);
    if(localfilepath.isEmpty())
    {
        //QMessageBox::warning(this, "Download File Canceled", "File Save Path is Empty!");
        return;
    }

    QFile *fp = new QFile(localfilepath);

    if(!fp->open(QFile::WriteOnly))
    {
        QMessageBox::warning(this, "下载文件错误", "无法打开文件");
        return;
    }

    QListWidgetItem *item = new QListWidgetItem();
    item->setText("DOWN+" + localfilepath);
    ui->taskListWidget->addItem(item);
    item->setSizeHint(QSize(0, 70));

    int64_t filesize = msg["filesize"].asInt64();
    std::shared_ptr<QFile> fptr(fp);

    TASKProgressBar *taskbar = new TASKProgressBar(filesize, localfilepath, remotefilepath, TASKProgressBar::DOWN);
    ui->taskListWidget->setItemWidget(item, taskbar);
    taskbar->setFormat("等待下载中 " + localfilepath);

    std::map<std::tuple<QString, uint16_t, int64_t>, std::list<BLOCKTASKPtr>> tasks;
    int64_t offset = 0;
    for(auto& block : blocks)
    {
        QString host = QString::fromStdString(block["chunkip"].asString());
        uint16_t port = block["chunkport"].asUInt();
        std::string blockhash = block["blockhash"].asString();
        int64_t length = block["blocksize"].asInt64();
        BLOCKTASKPtr btaskptr = std::make_shared<BLOCKTASK>(blockhash, fptr, offset, length, BLOCKTASK::DOWN);
        btaskptr->moveToThread(&workThread);
        connect(btaskptr.get(), &BLOCKTASK::readyDataSize, taskbar, &TASKProgressBar::handleData);

        tasks[std::make_tuple(host, port, 0)].push_back(btaskptr);
        offset += length;
    }

    for(auto& x : tasks)
    {
        UPDOWNER *downer = new UPDOWNER(std::get<0>(x.first), std::get<1>(x.first), x.second);
        downer->moveToThread(&workThread);
        connect(downer, &UPDOWNER::readyFinshed, downer, &UPDOWNER::deleteLater);
        connect(taskbar, &TASKProgressBar::startTask, downer, &UPDOWNER::start);
    }

    connect(taskbar, &TASKProgressBar::readyTaskOver, [this, taskbar]()
    {
        //下载完成
        QSound::play(":/music/downover");
        taskbar->setTaskOver("下载 " + taskbar->localfilepath + " 下载完成");
    });

    emit taskbar->startTask();

}

void MainWindow::on_upfileBtn_clicked()
{
    QString localFilepath = ui->localFilepathEdit->text();
    QFileInfo finfo(localFilepath);
    if(!finfo.isFile() || !finfo.isReadable())
    {
        QMessageBox::information(this, "上传文件错误", "无法读取文件");
        return;
    }
    QString remoteFilepath = ui->remoteFilepathEdit->text();

    if(this->upMap.find(remoteFilepath.toStdString()) != this->upMap.end())
    {
        QMessageBox::warning(this, "上传文件失败", "存在同名文件上传任务");
        return;
    }
    Json::Value upinfo;
    upinfo["localfilepath"] = localFilepath.toStdString();
    upinfo["remotefilepath"] = remoteFilepath.toStdString();
    upinfo["status"] = "checkfilename";
    this->upMap[remoteFilepath.toStdString()] = upinfo;
    Json::FastWriter fw;
    Json::Value req;
    req["filename"] = remoteFilepath.toStdString();
    req["filehash"] = "_____";
    this->master->send(UPFILE_REQ, fw.write(req));
}

void MainWindow::upfileMessage(const std::string& message)
{
    Json::Reader rd;
    Json::Value msg;
    rd.parse(message, msg);
    std::string remoteFilename = msg["filename"].asString();
    auto it = this->upMap.find(remoteFilename);
    if(it == this->upMap.end())
        return;
    Json::Value& upinfo = it->second;
    if(!upinfo.isMember("status"))
        return;
    QString localfilepath = QString::fromStdString(upinfo["localfilepath"].asString());
    if(upinfo["status"]=="checkfilename")
    {
        if(msg.isMember("error"))
        {
            QMessageBox::warning(this, "上传文件失败",
                                 "服务端已存在同名文件:" + QString::fromStdString(remoteFilename));
            this->upMap.erase(remoteFilename);
            return;
        }
        else
        {
            //创建进度条  计算文件hash
            upinfo["status"] = "checkfilehash";
            QListWidgetItem *item = new QListWidgetItem();
            item->setText("UP+" + localfilepath);
            ui->taskListWidget->addItem(item);
            item->setSizeHint(QSize(0, 70));

            QFile ff(localfilepath);
            TASKProgressBar *taskbar = new TASKProgressBar(ff.size(), localfilepath,
                                                           QString::fromStdString(remoteFilename), TASKProgressBar::CALCFILEHASH);
            ui->taskListWidget->setItemWidget(item, taskbar);
            taskbar->setFormat("等待计算文件HASH中 " + localfilepath);

            CalcHashTask *hashTask = new CalcHashTask(localfilepath, QString::fromStdString(remoteFilename), CalcHashTask::FILEHASH);
            hashTask->moveToThread(&hashThread);

            connect(taskbar, &TASKProgressBar::startTask, hashTask, &CalcHashTask::calcHash);
            connect(hashTask, &CalcHashTask::readyProgress, taskbar, &TASKProgressBar::handleData);
            connect(hashTask, &CalcHashTask::readyOver, this, &MainWindow::calcFileHashOver);

            emit taskbar->startTask();
        }
        return;
    }

    if(upinfo["status"]=="checkfilehash" && msg.isMember("fastpass"))
    {
        if(msg["fastpass"] == 1)
        {
            //秒传成功
            auto items = ui->taskListWidget->findItems("UP+"+localfilepath, Qt::MatchFixedString);
            if(items.size() != 1) return;
            auto item = items.front();
            item->setText("OVER+" + item->text());
            TASKProgressBar *taskbar = static_cast<TASKProgressBar*>(ui->taskListWidget->itemWidget(item));
            taskbar->setTaskOver("上传 " + localfilepath + " 秒传完成");
            this->upMap.erase(remoteFilename);
            this->master->send(FILELIST_REQ);
            return;
        }
        else
        {
            //计算文件块hash
            upinfo["status"] = "checkblockhash";

            auto items = ui->taskListWidget->findItems("UP+"+localfilepath, Qt::MatchFixedString);
            if(items.size() != 1) return;
            auto item = items.front();
            ui->taskListWidget->removeItemWidget(item);

            QFile ff(localfilepath);
            TASKProgressBar *taskbar = new TASKProgressBar(ff.size(), localfilepath,
                                                           QString::fromStdString(remoteFilename), TASKProgressBar::CALCBLOCKHASH);
            ui->taskListWidget->setItemWidget(item, taskbar);
            taskbar->setFormat("等待计算块HASH中 " + localfilepath);

            int64_t BLOCKSIZE = 64*1024*1024;
            if(msg.isMember("blocksize") && msg["blocksize"].isInt64())
                BLOCKSIZE = msg["blocksize"].asInt64();
            CalcHashTask *hashTask = new CalcHashTask(localfilepath, QString::fromStdString(remoteFilename), CalcHashTask::BLOCKHASH, BLOCKSIZE);
            hashTask->moveToThread(&hashThread);

            connect(taskbar, &TASKProgressBar::startTask, hashTask, &CalcHashTask::calcHash);
            connect(hashTask, &CalcHashTask::readyProgress, taskbar, &TASKProgressBar::handleData);
            connect(hashTask, &CalcHashTask::readyOver, this, &MainWindow::calcFileBlockHashOver);

            emit taskbar->startTask();
        }
    }
}

void MainWindow::calcFileHashOver(Json::Value fileinfo)
{
    if(fileinfo.isMember("error")) return;
    fileinfo.removeMember("localfilepath");
    std::string remoteFilename = fileinfo["filename"].asString();
    auto it = this->upMap.find(remoteFilename);
    if(it == this->upMap.end())
        return;
    Json::Value& upinfo = it->second;
    if(!upinfo.isMember("status"))
        return;
    upinfo["fileinfo"]["filehash"] = fileinfo["filehash"];
    Json::FastWriter fw;
    this->master->send(UPFILE_REQ, fw.write(fileinfo));
}

void MainWindow::calcFileBlockHashOver(Json::Value fileinfo)
{
    if(fileinfo.isMember("error")) return;
    fileinfo.removeMember("localfilepath");
    std::string remoteFilename = fileinfo["filename"].asString();
    auto it = this->upMap.find(remoteFilename);
    if(it == this->upMap.end())
        return;
    Json::Value& upinfo = it->second;
    if(!upinfo.isMember("status"))
        return;
    fileinfo["filehash"] = upinfo["fileinfo"]["filehash"];
    Json::FastWriter fw;
    this->master->send(UPFILE_BLOCK_REQ, fw.write(fileinfo));
}

void MainWindow::upfileBlockMessage(const std::string& message)
{
    //创建上传任务
    Json::Reader rd;
    Json::Value msg;
    rd.parse(message, msg);
    std::string remoteFilename = msg["filename"].asString();
    auto it = this->upMap.find(remoteFilename);
    if(it == this->upMap.end())
        return;
    Json::Value& upinfo = it->second;
    if(!upinfo.isMember("status"))
        return;
    QString localfilepath = QString::fromStdString(upinfo["localfilepath"].asString());

    auto items = ui->taskListWidget->findItems("UP+"+localfilepath, Qt::MatchFixedString);
    if(items.size() != 1)
    {
        qDebug() << "ERROR---------";
        return;
    }
    auto item = items.front();
    ui->taskListWidget->removeItemWidget(item);

    if(msg.isMember("error"))
    {
        TASKProgressBar *taskbar = static_cast<TASKProgressBar*>(ui->taskListWidget->itemWidget(item));
        taskbar->setTaskOver("上传 " + localfilepath + " 无法上传(server error)");
        this->upMap.erase(remoteFilename);
        this->master->send(FILELIST_REQ);
        return;
    }

    QFile *fp = new QFile(localfilepath);

    if(!fp->open(QFile::ReadOnly))
    {
        QMessageBox::warning(this, "上传文件错误", "无法打开文件");
        return;
    }

    int64_t filesize = fp->size();
    std::shared_ptr<QFile> fptr(fp);

    TASKProgressBar *taskbar = new TASKProgressBar(filesize, localfilepath,
                                                   QString::fromStdString(remoteFilename), TASKProgressBar::UP);
    ui->taskListWidget->setItemWidget(item, taskbar);
    taskbar->setFormat("等待上传文件中 " + localfilepath);

    int64_t offset = 0;
    std::map<std::tuple<QString, uint16_t, int64_t>, std::list<BLOCKTASKPtr>> tasks;
    for(auto& block : msg["blocks"])
    {
        int64_t length = block["blocksize"].asInt64();
        if(block.isMember("chunkip"))
        {
            QString host = QString::fromStdString(block["chunkip"].asString());
            uint16_t port = block["chunkport"].asUInt();
            std::string blockhash = block["blockhash"].asString();
            BLOCKTASKPtr btaskptr = std::make_shared<BLOCKTASK>(blockhash, fptr, offset, length, BLOCKTASK::UP);
            btaskptr->moveToThread(&workThread);
            connect(btaskptr.get(), &BLOCKTASK::readyDataSize, taskbar, &TASKProgressBar::handleData);
            tasks[std::make_tuple(host, port, 0)].push_back(btaskptr);
        }
        offset += length;
    }

    for(auto& x : tasks)
    {
        UPDOWNER *uper = new UPDOWNER(std::get<0>(x.first), std::get<1>(x.first), x.second);
        uper->moveToThread(&workThread);
        connect(uper, &UPDOWNER::readyFinshed, uper, &UPDOWNER::deleteLater);
        connect(taskbar, &TASKProgressBar::startTask, uper, &UPDOWNER::start);
    }

    connect(taskbar, &TASKProgressBar::finshed, this, &MainWindow::upfileBlockOver);
    Json::Value& fileinfo = upinfo["fileinfo"];
    fileinfo["blocks"] = msg["blocks"];
    fileinfo["filename"] = msg["filename"];
    fileinfo["filesize"] = msg["filesize"];
    fileinfo["filehash"] = msg["filehash"];

    emit taskbar->startTask();
}

void MainWindow::upfileBlockOver(QString remotefilepath)
{
    auto it = this->upMap.find(remotefilepath.toStdString());
    if(it == this->upMap.end())
        return;
    Json::Value& upinfo = it->second;
    if(!upinfo.isMember("status"))
        return;
    Json::FastWriter fw;
    this->master->send(ADDFILE_REQ, fw.write(upinfo["fileinfo"]));
}

void MainWindow::addFileRes(const std::string &message)
{
    Json::Reader rd;
    Json::Value msg;
    rd.parse(message, msg);
    if(msg.isMember("error")) return;

    std::string remoteFilename = msg["filename"].asString();
    auto it = this->upMap.find(remoteFilename);
    if(it == this->upMap.end())
        return;
    Json::Value& upinfo = it->second;
    if(!upinfo.isMember("status"))
        return;
    QString localfilepath = QString::fromStdString(upinfo["localfilepath"].asString());

    auto items = ui->taskListWidget->findItems("UP+"+localfilepath, Qt::MatchFixedString);
    if(items.size() != 1)
    {
        qDebug() << "ERROR---------";
        return;
    }
    auto item = items.front();
    item->setText("OVER+" + item->text());
    TASKProgressBar *taskbar = static_cast<TASKProgressBar*>(ui->taskListWidget->itemWidget(item));
    taskbar->setTaskOver("上传 " + localfilepath + " 上传完成");
    this->upMap.erase(remoteFilename);
    this->master->send(FILELIST_REQ);
}

void MainWindow::on_taskListWidget_customContextMenuRequested(const QPoint &pos)
{
    QListWidgetItem *curItem = ui->taskListWidget->itemAt(pos);
    if(curItem == nullptr)
    {
        QMenu *popMenu = new QMenu;

        QAction *clear = new QAction(tr("清空"), popMenu);
        popMenu->addAction(clear);

        connect(clear, &QAction::triggered, [this]()
        {
            this->ui->taskListWidget->clear();
        });

        popMenu->exec(QCursor::pos());
        delete popMenu;
    }
    else
    {
        QMenu *popMenu = new QMenu;

        QAction *deleteTask = new QAction(tr("删除"), popMenu);
        popMenu->addAction(deleteTask);
        connect(deleteTask, &QAction::triggered, [this, curItem]()
        {
            this->ui->taskListWidget->removeItemWidget(curItem);
            delete curItem;
        });

        ui->taskListWidget->itemWidget(curItem);

        popMenu->exec(QCursor::pos());
        delete popMenu;
    }
}

void MainWindow::on_taskListWidget_itemDoubleClicked(QListWidgetItem *item)
{
    TASKProgressBar *taskbar = static_cast<TASKProgressBar*>(ui->taskListWidget->itemWidget(item));
    if(taskbar->type == taskbar->DOWN)
        QDesktopServices::openUrl(QUrl(taskbar->localfilepath));
}
