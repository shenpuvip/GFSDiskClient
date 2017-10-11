
LIBS += -ljsoncpp

SOURCES += \
    $$PWD/main.cpp \
    $$PWD/mainwindow.cpp \
    $$PWD/uploader.cpp \
    $$PWD/startdialog.cpp

HEADERS += \
    $$PWD/mainwindow.h \
    $$PWD/msgcode.h \
    $$PWD/mtcpsocket.h \
    $$PWD/mastersocket.h \
    $$PWD/updowner.h \
    $$PWD/task.h \
    $$PWD/calchash.h \
    $$PWD/startdialog.h

FORMS += \
    $$PWD/mainwindow.ui \
    $$PWD/startdialog.ui

RESOURCES += \
    $$PWD/icon.qrc
