#include "LcdClient.hpp"

LcdClient::LcdClient(QObject *parent)
    : QObject(parent)
{
    connect(&updateTimer, &QTimer::timeout, this, &LcdClient::update);

    connect(&lcdSocket, &QIODevice::readyRead, this, &LcdClient::readServerResponse);
    connect(&lcdSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &LcdClient::handleSocketError);
    lcdSocket.abort();
    lcdSocket.connectToHost("127.0.0.1", 13666);

    lcdSocket.write("hello\n");
}

void LcdClient::update()
{
    QHash<QString, QList<QHostAddress>> ifaceIPs;
    QList<QNetworkInterface> allInterfaces = QNetworkInterface::allInterfaces();
    QNetworkInterface iface;
    QString textToBeScrolled;

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

            textToBeScrolled +=  iface.name() + ":";
            QHostAddress adr;
            foreach (adr, addresses) {
                textToBeScrolled += adr.toString() + ",";
            }
        }
    }
    textToBeScrolled = textToBeScrolled.trimmed();
    if (textToBeScrolled.endsWith(',')) {
        textToBeScrolled.chop(1);
    }

    qDebug() << textToBeScrolled;

    lcdSocket.write(QString("widget_set main line1 1 1 15 1 m 2 %2\n").arg(textToBeScrolled).toUtf8());
    lcdSocket.write(QString("widget_set main line2 1 2 %1?TxxCxxx\n").arg(QTime::currentTime().toString("HH:mm:ss")).toUtf8());
}

void LcdClient::readServerResponse()
{
    QString response = lcdSocket.readAll();
    qDebug() << "LCDd resp:" << response;

    if (response.startsWith("connect ")) {
        lcdSocket.write("screen_add main\n");
        lcdSocket.write("widget_add main line1 scroller\n");
        lcdSocket.write("widget_add main line2 string\n");
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