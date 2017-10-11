#include "startdialog.h"
#include "ui_startdialog.h"

#include "mainwindow.h"
#include "mastersocket.h"
#include <QMessageBox>

StartDialog::StartDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StartDialog)
{
    ui->setupUi(this);
    ui->hostEdit->setText("10.0.0.201");
}

StartDialog::~StartDialog()
{
    delete ui;
}

void StartDialog::on_connectBtn_clicked()
{
    QString host = ui->hostEdit->text();
    uint16_t port = ui->portBox->value();
    MasterSocket *master = new MasterSocket(host, port);
    master->connectToMaster();
    if(master->waitForConnected(3000))
    {
        MainWindow *mainwindow = new MainWindow(host, port, master);
        mainwindow->show();
        mainwindow->setAttribute(Qt::WA_DeleteOnClose);

        this->close();
    }
    else
    {
        QMessageBox::warning(this, "无法连接服务器", "无法连接到Master服务器！");
        master->deleteLater();
    }
}
