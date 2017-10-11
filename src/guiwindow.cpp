#include "guiwindow.h"
#include "ui_startwindow.h"
#include "ui_mainwindow.h"

#include <string>
#include <json/json.h>

#include <QMetaType>
#include <QCryptographicHash>
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>
#include <QMenu>
#include <QAction>

GuiWindow::GuiWindow(QWidget *parent) :
    QMainWindow(parent)
{
    sui = nullptr;
    ui = nullptr;

    sui = new Ui::StartWindow;
    ui = new Ui::MainWindow;
    sui->setupUi(this);
    sui->hostEdit->setText(masterHostname);
    sui->portEdit->setText(QString::number(masterPort));

    workThread.start();

    qRegisterMetaType<TASKRESULT>("TASKRESULT");
    qRegisterMetaType<TASK>("TASK");
}

GuiWindow::~GuiWindow()
{
    if(sui)
        delete sui;
    if(ui)
        delete ui;
    workThread.quit();
    workThread.wait();
}

void GuiWindow::on_connectBtn_clicked()
{
    QString hostname = sui->hostEdit->text();
    uint16_t port = sui->portEdit->text().toInt();
    mastersocket.reset(new MTcpSocket);
    mastersocket->connectToHost(hostname, port);
    if(!mastersocket->waitForConnected())
    {
        QMessageBox::warning(this, "can't connect", "can't connect to server!");
        return;
    }
    mastersocket->CONNECTREADY();
    connect(mastersocket.get(), &MTcpSocket::onMessage, this, &GuiWindow::onMasterMessage);
    connect(mastersocket.get(), &MTcpSocket::disconnected, this, &GuiWindow::onMasterDisConnect);
    mastersocket->send(AUTH_REQ);

    masterHostname = hostname;
    masterPort = port;

    int x = this->x() + this->width() / 2;
    int y = this->y() + this->height() / 2;
    ui->setupUi(this);
    x -= this->width() / 2;
    y -= this->height() / 2;
    this->move(x, y);

    initMainWindow();
    on_refreshBtn_clicked();
}

void GuiWindow::initMainWindow()
{
    ui->fileTable->setColumnCount(2);
    QStringList header;
    header << "FileName" << "FileSize";
    ui->fileTable->setHorizontalHeaderLabels(header);
    ui->fileTable->horizontalHeader()->setStretchLastSection(true);
    ui->fileTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    //ui->fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    //ui->fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->fileTable->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->fileTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}


void GuiWindow::on_refreshBtn_clicked()
{
    mastersocket->send(FILELIST_REQ);
}

void GuiWindow::onMasterMessage(msgtype_t msgtype, std::string message)
{
    switch (msgtype)
    {
    case FILELIST_RES:
    {
        Json::Reader rd;
        Json::Value filelist;
        rd.parse(message, filelist);
        ui->fileTable->clearContents();
        ui->fileTable->setRowCount(filelist.size());
        int row = 0;
        for(auto& file : filelist)
        {
            std::string filename = file["filename"].asString();
            int64_t filesize = file["filesize"].asInt64();
            ui->fileTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(filename)));
            ui->fileTable->setItem(row, 1, new QTableWidgetItem(QString::number(filesize)));
            ++row;
        }
    }
        break;
    default:
        qDebug() << msgtype << QString::fromStdString(message);
    }
}

void GuiWindow::onMasterDisConnect()
{
    ;
}

void GuiWindow::on_upFileBtn_clicked()
{
    QString filepath = ui->localfilePathEdit->text();
    if(filepath.size() <= 0) return;
    QFileInfo fi(filepath);
    if(!fi.isFile()) return;

    TASK task;
    task.taskType = TASK::UP;
    task.localfilepath = filepath;
    task.remotefilepath = ui->remotefilePathEdit->text();
    WORKER *worker = new WORKER(masterHostname, masterPort, task);
    worker->moveToThread(&workThread);

    //创建进度条
    //QListWidgetItem *upfileItem = new QListWidgetItem(task.remotefilepath, ui->listWidget);

    connect(worker, &WORKER::resultReady, [](const TASKRESULT& result){
        qDebug() << "上传进度：" << result.nprogress << " " << result.status;
    });

    emit worker->startWork();
}

void GuiWindow::on_selectFileBtn_clicked()
{
    QString filepath = QFileDialog::getOpenFileName(this);
    if(!filepath.isEmpty())
    {
        ui->localfilePathEdit->setText(filepath);
        ui->remotefilePathEdit->setText(filepath.split('/').back());
    }
}

void GuiWindow::on_fileTable_customContextMenuRequested(const QPoint &pos)
{
    QTableWidgetItem *item = ui->fileTable->itemAt(pos);
    if(item == nullptr)
    {
        ;
    }
    else
    {
        int row = item->row();
        QString filename = ui->fileTable->item(row, 0)->text();
        if(filename.isEmpty())
            return;

        QMenu *popMenu = new QMenu(this);
        QAction *downfile = new QAction(tr("Download"), popMenu);
        popMenu->addAction(downfile);

        connect(downfile, &QAction::triggered, [this, filename]()
        {
            QString localfilepath = QFileDialog::getSaveFileName(NULL, "", filename);
            if(localfilepath.isEmpty())
                return;
            TASK task;
            task.taskType = TASK::DOWN;
            task.remotefilepath = filename;
            task.localfilepath = localfilepath;
            WORKER *worker = new WORKER(masterHostname, masterPort, task);
            worker->moveToThread(&workThread);
            connect(worker, &WORKER::resultReady, [](const TASKRESULT& result){
                qDebug() << "下载进度：" << result.nprogress << " " << result.status;
            });

            emit worker->startWork();
        });

        popMenu->exec(QCursor::pos());
        delete popMenu;
    }
}
