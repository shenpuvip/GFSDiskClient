#include "mainwindow.h"
#include "startdialog.h"
#include <QApplication>
#include <QMetaObject>
#include <QDebug>

#include <stdint.h>
#include <string>

int main(int argc, char *argv[])
{
    qRegisterMetaType<int64_t>("int64_t");
    qRegisterMetaType<Json::Value>("Json::Value");

    QApplication a(argc, argv);
    StartDialog start;
    start.show();

    return a.exec();
}
