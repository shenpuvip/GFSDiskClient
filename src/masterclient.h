#ifndef MASTERCLIENT_H
#define MASTERCLIENT_H

#include <QTcpSocket>

#include "msgcode.h"

class MasterClient : public QtcpSocket
{
    Q_OBJECT

public:
    MasterClient();

signal:
    onMessage();

private:
    ;
};

#endif // MASTERCLIENT_H
