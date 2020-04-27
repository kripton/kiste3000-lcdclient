#include "LcdClient.hpp"

LcdClient::LcdClient(QObject *parent)
    : QObject(parent)
{
    connect(&updateTimer, &QTimer::timeout, this, &LcdClient::update);

    fileTemp.setFileName("/sys/devices/virtual/thermal/thermal_zone0/temp");
    if (!fileTemp.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open thermal_zone file!";
    }

    fileStat.setFileName("/proc/stat");
    if (!fileStat.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open /proc/stat!";
    }

    connect(&lcdSocket, &QIODevice::readyRead, this, &LcdClient::readServerResponse);
    connect(&lcdSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &LcdClient::handleSocketError);
    lcdSocket.abort();
    lcdSocket.connectToHost("127.0.0.1", 13666);

    lcdSocket.write("hello\n");
}

void LcdClient::update()
{
    lcdSocket.write(QString("widget_set time line1 Uhrzeit\n").toLatin1());
    lcdSocket.write(QString("widget_set time line2 1 2 \"%1\"\n").arg(QDateTime::currentDateTime().toString("dd.MM.  HH:mm:ss")).toLatin1());

    lcdSocket.write(QString("widget_set sys line1 System\n").toLatin1());
    lcdSocket.write(QString("widget_set sys line2 1 2 \"CPU:%1  T:%2°C\"\n")
        .arg(getMachineCPULoad())
        .arg(getMachineTemp())
        .toLatin1()
    );

    lcdSocket.write(QString("widget_set net line1 eth0\n").toLatin1());
    lcdSocket.write(QString("widget_set net line2 1 2 \"%1\"\n")
        .arg(getMachineIPs().right(13))
        .toLatin1()
    );
}

QString LcdClient::getMachineCPULoad() {
    fileStat.seek(0);
    QString line = fileStat.readLine().trimmed();
    QStringList parts = line.split(' ', QString::SkipEmptyParts);

    loadStruct currentLoad;
    unsigned long diffUser, diffSystem, diffNice, diffTotal;

    currentLoad.user = parts[1].toULong();
    currentLoad.nice = parts[2].toULong();
    currentLoad.system = parts[3].toULong();
    currentLoad.idle = parts[4].toULong();
    currentLoad.idle += parts[5].toULong();     // iowait
    currentLoad.system += parts[6].toULong();   // irq
    currentLoad.system += parts[7].toULong();   // softirq

    currentLoad.total = currentLoad.user + currentLoad.nice + currentLoad.system + currentLoad.idle;

    diffUser = currentLoad.user - lastLoad.user;
    diffSystem = currentLoad.system - lastLoad.system;
    diffNice = currentLoad.nice - lastLoad.nice;
    diffTotal = currentLoad.total - lastLoad.total;

    lastLoad = currentLoad;

    int cpuLoad = qRound(100.0 * (((double) diffUser + (double) diffSystem + (double) diffNice) / (double) diffTotal));

    return QString("%1\%").arg((int)cpuLoad, 3, 10, QLatin1Char('0'));
}

QString LcdClient::getMachineTemp() {
    fileTemp.seek(0);
    float temp = QString(fileTemp.readAll()).toFloat() / 1000;
    return QString("%1°C").arg(qRound(temp));
}

// Generates a string containing all "external" network interfaces and their IPv4 addresses
QString LcdClient::getMachineIPs() {
    QHash<QString, QList<QHostAddress>> ifaceIPs;
    QList<QNetworkInterface> allInterfaces = QNetworkInterface::allInterfaces();
    QNetworkInterface iface;
    QString machineIPs;

    foreach(iface, allInterfaces) {
        QList<QNetworkAddressEntry> allEntries = iface.addressEntries();
        QList<QHostAddress> addresses;
        QNetworkAddressEntry entry;
        foreach (entry, allEntries) {
            if (!entry.ip().isLoopback() &&
                !entry.ip().isMulticast() &&
                (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) &&
                !iface.name().startsWith("docker")
            ) {
                addresses.append(entry.ip());
            }
        }

        if (addresses.count()) {
            ifaceIPs.insert(iface.name(), addresses);

            machineIPs +=  " " + iface.name() + ":";
            QHostAddress adr;
            foreach (adr, addresses) {
                machineIPs += adr.toString() + ",";
            }
            if (machineIPs.endsWith(',')) {
                machineIPs.chop(1);
            }
        }
    }
    machineIPs = machineIPs.trimmed();

    return machineIPs;
}

void LcdClient::readServerResponse()
{
    QString response = lcdSocket.readAll();
    qDebug() << "LCDd resp:" << response;

    if (response.startsWith("connect ")) {
        lcdSocket.write("screen_add time\n");
        lcdSocket.write("widget_add time line1 title\n");
        lcdSocket.write("widget_add time line2 string\n");
        lcdSocket.write("screen_add sys\n");
        lcdSocket.write("widget_add sys line1 title\n");
        lcdSocket.write("widget_add sys line2 string\n");
        lcdSocket.write("screen_add net\n");
        lcdSocket.write("widget_add net line1 title\n");
        lcdSocket.write("widget_add net line2 string\n");
        updateTimer.start(1000);
    }
}

void LcdClient::handleSocketError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        break;
    case QAbstractSocket::HostNotFoundError:
        break;
    case QAbstractSocket::ConnectionRefusedError:
        break;
    default:
        break;
    }
}