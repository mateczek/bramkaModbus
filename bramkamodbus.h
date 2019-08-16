#ifndef BRAMKAMODBUS_H
#define BRAMKAMODBUS_H

#include <QObject>
#include<QTcpServer>
#include<QTcpSocket>
#include<QTimer>
enum _tryb{
  RTU_TcpIP=1,
  RTU_RS=2
};

class bramkaModbus : public QObject{
    Q_OBJECT
    int licznik_Klientow=0;
    static const uint8_t table_crc_hi[];
    static const uint8_t table_crc_lo[];
    _tryb trybTransmisji;
    QIODevice* remoteDevice;
    void setupRTU_RS232(QTextStream& ts);
    void setupRTU_TCP(QTextStream&ts);
    void QuitAPP(QString message);
public:
    QTcpServer *serwer;
    void startSerwer(uint16_t port);
    explicit bramkaModbus(QObject *parent = nullptr);
    ~bramkaModbus();

signals:
    void quit();
private slots:

    void newConnection();//nowy klient
    void removeClient(); //tylko informacyjnie
    void modbusQuestionIncoming();
    void calcCRC16(QByteArray&frame);
    bool checkCRC16(QByteArray &frame);
};
#endif // BRAMKAMODBUS_H
