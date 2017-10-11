#ifndef GUIWINDOW_H
#define GUIWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QThread>

#include <memory>

#include "worker.h"
#include "mtcpsocket.h"
#include "msgcode.h"

namespace Ui {
class StartWindow;
class MainWindow;
}

class GuiWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit GuiWindow(QWidget *parent = 0);
    ~GuiWindow();

private slots:
    void on_connectBtn_clicked();
    void on_refreshBtn_clicked();
    void on_upFileBtn_clicked();
    void on_selectFileBtn_clicked();

    void on_fileTable_customContextMenuRequested(const QPoint &pos);

private:
    void initMainWindow();
    void onMasterMessage(msgtype_t msgtype, std::string message);
    void onMasterDisConnect();

private:
    Ui::StartWindow *sui;
    Ui::MainWindow *ui;

    QString masterHostname = "192.168.15.151";
    uint16_t masterPort = 61481;
    std::shared_ptr<MTcpSocket> mastersocket;

    QThread workThread;
};


#endif // GUIWINDOW_H
