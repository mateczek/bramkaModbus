/*copyright 2019 mateczek
Ten plik jest częścią bramkaModbus.

    bramkaModbus jest wolnym oprogramowaniem: możesz go rozprowadzać dalej
    i/lub modyfikować na warunkach Powszechnej Licencji Publicznej GNU,
    wydanej przez Fundację Wolnego Oprogramowania - według wersji 3 tej
    Licencji lub (według twojego wyboru) którejś z późniejszych wersji.

    bramkaModbus rozpowszechniany jest z nadzieją, iż będzie on
    użyteczny - jednak BEZ JAKIEJKOLWIEK GWARANCJI, nawet domyślnej
    gwarancji PRZYDATNOŚCI HANDLOWEJ albo PRZYDATNOŚCI DO OKREŚLONYCH
    ZASTOSOWAŃ. W celu uzyskania bliższych informacji sięgnij do     Powszechnej Licencji Publicznej GNU.
    <http://www.gnu.org/licenses/>.
*/

#include "bramkamodbus.h"
#include<QFile>
#include<QTextStream>
#include<QSerialPort>
#include<QThread>
QTextStream& operator>>(QTextStream &ts, _tryb &t){
    int temp;
    ts >> temp;
    t=static_cast<_tryb>(temp);
    return ts;
};

void bramkaModbus::startSerwer(uint16_t port){
    serwer=new QTcpServer(this);
    if (!serwer->listen(QHostAddress::AnyIPv4, port)){
        QuitAPP("Server Modbus TCP");
        return;
    }
    qDebug()<< "Server Modbus TCP..............................OK";
    connect(serwer,SIGNAL(newConnection()),this,SLOT(newConnection())); //nowy klient incoming
}

bramkaModbus::bramkaModbus(QObject *parent) : QObject(parent){
    QFile plik("ustawienia.conf");
    if (!plik.open(QIODevice::ReadOnly | QIODevice::Text)){
        QuitAPP("settings file");
        return;
    }
    QTextStream ts(&plik);
    uint16_t port;
    ts>>trybTransmisji>>port;
    startSerwer(port);
    if(trybTransmisji==RTU_TcpIP) setupRTU_TCP(ts);
    else if(trybTransmisji==RTU_RS) setupRTU_RS232(ts);
}

bramkaModbus::~bramkaModbus(){
    //serwer->close();
    qDebug()<<"zamknięcie serwera";
}
void bramkaModbus::QuitAPP(QString message)
{
    int rozmiar=message.size();
    for(int i=rozmiar;i<60;i++) message+='.';
    message+="NOK";
    qDebug()<<message;
    QTimer::singleShot(500,[this](){emit quit();});
}
void bramkaModbus::setupRTU_RS232(QTextStream &ts){
    QString portname;int speed;int db;
    ts>>portname>>speed>>db;
    QSerialPort *portCom=new QSerialPort(portname,this);
    portCom->setBaudRate(speed);
    portCom->setDataBits(static_cast<QSerialPort::DataBits>(db));
    portCom->setParity(QSerialPort::NoParity);
    portCom->setStopBits(QSerialPort::OneStop);
    portCom->setFlowControl(QSerialPort::NoFlowControl);
    portCom->setReadBufferSize(9);
    if(portCom->open(QIODevice::ReadWrite)){
        remoteDevice=portCom;
        qDebug()<<"Connect to Device BY Modbus RTU ...............OK";
    }else{
        QuitAPP("Connect to Device BY Modbus RTU");
    }
}

void bramkaModbus::setupRTU_TCP(QTextStream &ts){
    QString ip;uint16_t port;
    ts>>ip>>port;
    QTcpSocket* Socket=new QTcpSocket(this);
    Socket->connectToHost(ip,port,QIODevice::ReadWrite);
    if(Socket->waitForConnected()){
        remoteDevice=Socket;
        qDebug()<<"Connect to Device BY Modbus RTU_OVER TCP ................OK";
    }else{
        QuitAPP("Connect to Device BY Modbus RTU_OVER TCP");
    }
}

void bramkaModbus::newConnection(){
    qDebug()<<"new client connected client count = "<<++licznik_Klientow;
    QTcpSocket* soc = serwer->nextPendingConnection();                     //gniazdo do komunikacji z klientem
    connect(soc,SIGNAL(readyRead()),this, SLOT(modbusQuestionIncoming())); //obsługa zapytania
    connect(soc,SIGNAL(disconnected()),this,SLOT(removeClient()));         //klient się zorłączył
}

void bramkaModbus::removeClient()
{
    qDebug()<<"one client disconnected client count = "<<--licznik_Klientow;
    QTcpSocket *soc= qobject_cast<QTcpSocket*>(sender());
    soc->deleteLater();
}

void bramkaModbus::modbusQuestionIncoming(){
    QTcpSocket * s=qobject_cast<QTcpSocket*>(sender());     //socket z którego przyszło zapytanie
    QByteArray tcpModbusQuestions = s->readAll();           //zapytanie modbusa
    QByteArray respondeFrame=tcpModbusQuestions.mid(0,5);   //zapamiętanie w ramce odpowiedzi numeru tranzakcji
    int size=tcpModbusQuestions[5];                         //wyciągnięcie bajtu odpowiadającego za rozmiar
    if (tcpModbusQuestions.size()!=size+6) return;          //sprawdzenie zgodności odebranej ramki z zadeklarowanym rozmiarem
    QByteArray rtuFrame=tcpModbusQuestions.mid(6);          //wyodrębnienie ramki RTU z ramki TCP. Ramka RTU zaczyna się od bajtu nr 6
    calcCRC16(rtuFrame);                                    //policzenie sumy kontrolnej ramki RTU. funkcja doda 2 bajty sumy kontrolnej
    remoteDevice->write(rtuFrame);                          //wysłanie ramki modubsa-RTU do urządzenia
    QByteArray uframe(5+rtuFrame[5]*2,0);                   //delkaracja ramki na odpowiedź (rozmiar ramki)
    for(int count=0;remoteDevice->bytesAvailable()<uframe.size();count++){ //czekanie na pełną ramkę
        remoteDevice->waitForReadyRead(10);
        if(count>10){
            qDebug()<<"the device is not responding";        //timeout, Urządzenie nie odpowiada
            return;
        }
    }
    uframe = remoteDevice->readAll();                       //odczytanie pełnej ramki odpowiedzi
    if(checkCRC16(uframe)){                                 //sprawdzenie sumy kontrolnej odebranej ramki
        respondeFrame+=uframe.size();                       //przygotowanie ramki odpowiedzi dla serwera modbus TCP (pole rozmiaru)
        respondeFrame+=uframe;                              //dodajemy do nagłówka modbusTCP ramkę modbusaRTU(bez sumy kontrolnej)
        s->write(respondeFrame);                            //odpowiedź do klienta.
        s->waitForBytesWritten();
   }
}

/*
 * Copyright © 2001-2011 Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
void  bramkaModbus::calcCRC16(QByteArray &frame)
{
    uint8_t crc_hi = 0xFF, crc_lo = 0xFF; /* low CRC byte initialized */
    unsigned int i;
    for (int count=0;count<frame.size();count++){
        i = crc_hi ^ static_cast<uint8_t>(frame[count]); /* calculate the CRC  */
        crc_hi = crc_lo ^ table_crc_hi[i];
        crc_lo = table_crc_lo[i];
    }
    frame+=static_cast<char>(crc_hi);
    frame+=static_cast<char>(crc_lo);
}

bool bramkaModbus::checkCRC16(QByteArray &frame){
    uint8_t crc_hi = 0xFF, crc_lo = 0xFF; /* low CRC byte initialized */
    unsigned int i;
    auto rozmiar = frame.size();
    for (int count=0;count<rozmiar-2;count++){
        i = crc_hi ^ static_cast<uint8_t>(frame[count]); /* calculate the CRC  */
        crc_hi = crc_lo ^ table_crc_hi[i];
        crc_lo = table_crc_lo[i];
    }
    if((frame[rozmiar-2]==static_cast<char>(crc_hi))&&(frame[rozmiar-1]==static_cast<char>(crc_lo))){
        frame.chop(2);
        return  true;
    }
    return false;
}

const uint8_t bramkaModbus::table_crc_hi[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40
};

/* Table of CRC values for low-order byte */
const uint8_t bramkaModbus::table_crc_lo[] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
    0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
    0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
    0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
    0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
    0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
    0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
    0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
    0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
    0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
    0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
    0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
    0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
    0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
    0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
    0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
    0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
    0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
    0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
    0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
    0x43, 0x83, 0x41, 0x81, 0x80, 0x40
};
