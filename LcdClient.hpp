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

    QString getMachineIPs();
};
#endif  // LCDCLIENT_H_
