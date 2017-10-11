#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <string>
#include <memory>
#include <map>

#include <json/json.h>
#include "mastersocket.h"
#include "task.h"

#include <QMainWindow>
#include <QThread>
#include <QListWidgetItem>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QString masterHost, uint16_t masterPort,
                        MasterSocket* mastersocket = nullptr,
                        QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_remoteFileListWidget_customContextMenuRequested(const QPoint &pos);
    void on_selectFileBtn_clicked();
    void on_upfileBtn_clicked();
    void on_taskListWidget_customContextMenuRequested(const QPoint &pos);

    void refreshFileList(const std::string& message);
    void downfileMessage(const std::string& message);
    void upfileMessage(const std::string& message);
    void upfileBlockMessage(const std::string& message);
    void upfileBlockOver(QString remotefilepath);
    void addFileRes(const std::string& message);

    void on_taskListWidget_itemDoubleClicked(QListWidgetItem *item);

    void calcFileHashOver(Json::Value fileinfo);
    void calcFileBlockHashOver(Json::Value fileinfo);

private:
    Ui::MainWindow *ui;

    QString masterHost;
    uint16_t masterPort;
    std::shared_ptr<MasterSocket> master;

    QThread workThread;
    QThread hashThread;
    std::map<std::string, Json::Value> upMap;
};

#endif // MAINWINDOW_H
