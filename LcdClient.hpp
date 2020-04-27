#include <QtNetwork>
#include <QtCore>

#include <QDebug>

#include <QTime>
#include <QTimer>
#include <QFile>
#include <QTcpSocket>
#include <QHash>

#include <QNetworkInterface>

#ifndef LCDCLIENT_H_
#define LCDCLIENT_H_

typedef struct
{
    unsigned long user;
    unsigned long system;
    unsigned long nice;
    unsigned long idle;
    unsigned long total;
} loadStruct;

class LcdClient : public QObject
{
    Q_OBJECT

public:
    explicit LcdClient(QObject *parent = nullptr);

private slots:
    void update();
    void readServerResponse();
    void handleSocketError(QAbstractSocket::SocketError socketError);

private:
    QTimer updateTimer;
    QTcpSocket lcdSocket;
    QFile fileTemp;
    QFile fileStat;

    loadStruct lastLoad;

    QString getMachineIPs();
    QString getMachineTemp();
    QString getMachineCPULoad();
};
#endif  // LCDCLIENT_H_
