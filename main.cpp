#include <QCoreApplication>

#include "LcdClient.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    LcdClient lcdClient;

    app.setApplicationName("kiste3000-lcdclient");

    return app.exec();
}
