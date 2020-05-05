#include <QtNetwork>
#include <QtCore>

#include <QDebug>

#include <QTime>
#include <QTimer>
#include <QFile>
#include <QTcpSocket>
#include <QHash>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QJsonDocument>
#include <QProcess>

#include <QNetworkInterface>

#ifndef LCDCLIENT_H_
#define LCDCLIENT_H_

// One set of CPU load values
// for calculating the difference
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
    void handleHttpResponse(QNetworkReply *reply);
    void readServerResponse();
    void handleSocketError(QAbstractSocket::SocketError socketError);

private:
    QTimer updateTimer;
    QTcpSocket lcdSocket;
    QFile fileTemp;
    QFile fileStat;
    QFile fileVoltAlarm;
    QFile fileTempThrottle;
    QNetworkAccessManager qnam;
    QNetworkRequest request;
    QProcess proc;

    // Used to switch backlight either
    // BLUE = all values of all universes = 0
    // GREEN = some value in any universe != 0
    int universeAllZeroes[8];

    // Offest for each universe screen storing the
    // first channel number to be displayed (0 based)
    int universeOffset[8];

    // Will add RED backlight if != 0
    int machineProblem;

    QString currentScreen;

    loadStruct lastLoad;

    QString getMachineIPs();
    QString getMachineTemp();
    QString getMachineCPULoad();
    QString getRPiStatus();
    void updateDmxUniverse(int universe);
    void updateBacklight();
};
#endif  // LCDCLIENT_H_
