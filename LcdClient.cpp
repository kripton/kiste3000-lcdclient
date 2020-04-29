#include "LcdClient.hpp"

// Constructor and initialization routines (Opening files, connecting to LCDd, ...)
LcdClient::LcdClient(QObject *parent)
    : QObject(parent)
{
    connect(&updateTimer, &QTimer::timeout, this, &LcdClient::update);

    connect(&qnam, &QNetworkAccessManager::finished, this, &LcdClient::handleHttpResponse);

    fileTemp.setFileName("/sys/devices/virtual/thermal/thermal_zone0/temp");
    if (!fileTemp.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open /sys/devices/virtual/thermal/thermal_zone0/temp";
    }

    fileStat.setFileName("/proc/stat");
    if (!fileStat.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open /proc/stat";
    }

    fileVoltAlarm.setFileName("/sys/devices/platform/soc/soc:firmware/raspberrypi-hwmon/hwmon/hwmon0/in0_lcrit_alarm");
    if (!fileVoltAlarm.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open /sys/devices/platform/soc/soc:firmware/raspberrypi-hwmon/hwmon/hwmon0/in0_lcrit_alarm";
    }

    fileTempThrottle.setFileName("/sys/devices/platform/soc/soc:firmware/get_throttled");
    if (!fileTempThrottle.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open /sys/devices/platform/soc/soc:firmware/get_throttled";
    }

    connect(&lcdSocket, &QIODevice::readyRead, this, &LcdClient::readServerResponse);
    connect(&lcdSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &LcdClient::handleSocketError);
    lcdSocket.abort();
    lcdSocket.connectToHost("127.0.0.1", 13666);

    lcdSocket.write("hello\n");
}

// Update the currently visible screen content
void LcdClient::update()
{
    if (currentScreen == "time") {
        lcdSocket.write(QString("widget_set time line1 Uhrzeit\n").toLatin1());
        lcdSocket.write(QString("widget_set time line2 1 2 \"%1\"\n")
            .arg(QDateTime::currentDateTime().toString("dd.MM.  HH:mm:ss"))
            .toLatin1()
        );
    } else if (currentScreen == "net") {
        lcdSocket.write(QString("widget_set net line1 Netzwerk\n").toLatin1());
        lcdSocket.write(QString("widget_set net line2 1 2 15 2 m 2 \"%1\"\n")
            .arg(getMachineIPs())
            .toLatin1()
        );
    } else if (currentScreen == "sys") {
        lcdSocket.write(QString("widget_set sys line1 \"Status:%1\"\n")
            .arg(getRPiStatus())
            .toLatin1()
        );
        lcdSocket.write(QString("widget_set sys line2 1 2 \"CPU:%1  T:%2°C\"\n")
            .arg(getMachineCPULoad())
            .arg(getMachineTemp())
            .toLatin1()
        );
    } else if (currentScreen.startsWith("universe")) {
        int universe = currentScreen.right(1).toInt();
        updateDmxUniverse(universe);
    }
}

// Calculate the current CPU usage
QString LcdClient::getMachineCPULoad()
{
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

// Read the current thermal_zone temperature
QString LcdClient::getMachineTemp()
{
    fileTemp.seek(0);
    float temp = QString(fileTemp.readAll()).toFloat() / 1000;
    return QString("%1°C").arg(qRound(temp));
}

// Generates a string containing all "external" network interfaces and their IPv4 addresses
QString LcdClient::getMachineIPs()
{
    // Step 1: Get a list of all network interfaces
    QHash<QString, QList<QHostAddress>> ifaceIPs;
    QList<QNetworkInterface> allInterfaces = QNetworkInterface::allInterfaces();
    QNetworkInterface iface;
    QString machineIPs;

    // Step 2: For each interface, get a list of all Addresses of that interface
    foreach(iface, allInterfaces) {
        QList<QNetworkAddressEntry> allEntries = iface.addressEntries();
        QList<QHostAddress> addresses;
        QNetworkAddressEntry entry;
        // Step 3: For each address, find all
        //         Non-Loopback, Non-Multicast IPv4
        //         and also interface name must not start with "docker"
        foreach (entry, allEntries) {
            if (!entry.ip().isLoopback() &&
                !entry.ip().isMulticast() &&
                (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) &&
                !iface.name().startsWith("docker")
            ) {
                addresses.append(entry.ip());
            }
        }

        // Step 4: If an address remained for that interface,
        //         add it + the IPs to the final QHash (currently unused)
        //         and a string-representation thereof (that is being returned)
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

// Get the RPi status (temp throttle and undervoltage)
QString LcdClient::getRPiStatus()
{
    fileVoltAlarm.seek(0);
    fileTempThrottle.seek(0);

    int voltAlarm = QString(fileVoltAlarm.readAll()).toInt();
    int tempThrottle = QString(fileTempThrottle.readAll()).toInt();

    if (voltAlarm && !tempThrottle) {
        return QString("Saft");
    } else if (!voltAlarm && tempThrottle) {
        return QString("Hitz");
    } else if (voltAlarm && tempThrottle) {
        return QString("H&S");
    } else {
        return QString("Gut");
    }
}

// Parse responses from LCDd (via TCP socket)
void LcdClient::readServerResponse()
{
    QString response = lcdSocket.readAll();
    QStringList lines = response.split("\n", QString::SkipEmptyParts);

    QString line;
    foreach(line, lines) {
        if (line == "success") {
            continue;
        }
        qDebug() << "LCDd resp:" << line;

        if (line.startsWith("connect ")) {
            lcdSocket.write("screen_add time\n");
            lcdSocket.write("widget_add time line1 title\n");
            lcdSocket.write("widget_add time line2 string\n");
            lcdSocket.write("screen_add sys\n");
            lcdSocket.write("widget_add sys line1 title\n");
            lcdSocket.write("widget_add sys line2 string\n");
            lcdSocket.write("screen_add net\n");
            lcdSocket.write("widget_add net line1 title\n");
            lcdSocket.write("widget_add net line2 scroller\n");
            for (int i = 1; i <= 8; i++) {
                lcdSocket.write(QString("screen_add universe%1\n").arg(i).toLatin1());
                lcdSocket.write(QString("widget_add universe%1 line1 title\n").arg(i).toLatin1());
                lcdSocket.write(QString("widget_add universe%1 line2 string\n").arg(i).toLatin1());
                lcdSocket.write(QString("widget_set universe%1 line1 \"Universe %1\"\n").arg(i).toLatin1());
            }
            updateTimer.start(250);
        } else if (line.startsWith("listen")) {
            currentScreen = line.split(" ")[1].trimmed();
            update();
        }
    }
}

// Request the current DMX values of a given universe ID from OLAd
void LcdClient::updateDmxUniverse(int universe) {
    request.setUrl(QUrl(QString("http://127.0.0.1:9090/get_dmx?u=%1").arg(universe)));
    qnam.get(request);
}

// Parse the HTTP response sent from OLAd (DMX values)
void LcdClient::handleHttpResponse(QNetworkReply *reply)
{
    if (reply->error()) {
        qDebug() << reply->errorString();
        return;
    }

    int universe = reply->url().toString().right(1).toInt();

    QString answer = reply->readAll();
    QJsonArray jsonArray = QJsonDocument::fromJson(answer.toUtf8())["dmx"].toArray();

    //qDebug() << "ID:" << universe << ": " << jsonArray[0].toInt();

    lcdSocket.write(QString("widget_set universe%1 line2 1 2 \"%2%3%4%5%6%7%8%9\"\n")
        .arg(universe)
        .arg(jsonArray[0].toInt(), 2, 16, QLatin1Char('0'))
        .arg(jsonArray[1].toInt(), 2, 16, QLatin1Char('0'))
        .arg(jsonArray[2].toInt(), 2, 16, QLatin1Char('0'))
        .arg(jsonArray[3].toInt(), 2, 16, QLatin1Char('0'))
        .arg(jsonArray[4].toInt(), 2, 16, QLatin1Char('0'))
        .arg(jsonArray[5].toInt(), 2, 16, QLatin1Char('0'))
        .arg(jsonArray[6].toInt(), 2, 16, QLatin1Char('0'))
        .arg(jsonArray[7].toInt(), 2, 16, QLatin1Char('0'))
        .toLatin1()
    );
}

// Handle socket errors on LCDd communication socket
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