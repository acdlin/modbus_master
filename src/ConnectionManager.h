#pragma once

#include <QObject>
#include <QSerialPort>
#include <QTcpSocket>
#include "ModbusClient.h"

class ConnectionManager : public QObject
{
    Q_OBJECT
public:

    ConnectionManager(QObject* parent = nullptr);
    ~ConnectionManager();
    enum Mode {RTU , TCP};
    enum ErrorType { ConnectionError, TimeoutError, CrcError, ExceptionError };
    void setMode(Mode mode);
    void setSerialConfig(const QString& port , int baud , int dataBits , int stopBits , int parity);
    void setTcpConfig(const QString& host , int port);
    void setModbusClient(ModbusClient* client);

public slots:
    void connectToDevice();
    void disconnectFromDevice();
    QByteArray sendAndWait(const QByteArray& request, int timeoutMs = 500);

signals:
    void connected();
    void disconnected();
    void error(const QString& msg , ErrorType type);
    void frameSent(const QByteArray& frame);
    void frameReceived(const QByteArray& frame);
    void frameError(const QString& msg , ErrorType type);

private:
    QSerialPort* m_serial;
    QTcpSocket* m_socket;
    QString m_tcpHost;
    int m_tcpPort;
    Mode m_mode;
    int m_dataBits = 8;
    int m_stopBits = 1;
    int m_parity = 0;
    quint16 m_transactionId = 0;
    ModbusClient* m_modbusClient = nullptr;
};