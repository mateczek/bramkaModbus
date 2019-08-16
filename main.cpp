#include <QCoreApplication>
#include<bramkamodbus.h>
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    bramkaModbus bm;
    QObject::connect(&bm,SIGNAL(quit()),&a,SLOT(quit()));
    return a.exec();
}
