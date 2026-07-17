#pragma once
#include <QObject>
#include <QList>

    struct Response {
      bool valid = false;
      int slaveId;
      int funcCode;
      int startAddr;
      int errorCode = 0;
      QList<quint16> values;
      QString errorString;
    };

  class ModbusClient : public QObject {
    Q_OBJECT
  public:
    // 构建请求帧
    ModbusClient(QObject* parent = nullptr);
    ~ModbusClient();
    QByteArray buildReadRequest(int slaveId, int funcCode, int startAddr, int count);
    QByteArray buildWriteSingleRequest(int slaveId,  int addr, quint16 value);
    QByteArray buildWriteMultiRequest(int slaveId, int funcCode, int startAddr, const QList<quint16>& values);
    QByteArray wrapMbapHeader(const QByteArray& rtuFrame, quint16 transactionId);
    QByteArray unwrapMbapHeader(const QByteArray& tcpFrame, quint16& transactionId);
    Response parseTcpResponse(const QByteArray& raw);
    Response parseResponse(const QByteArray& raw);

    // CRC16 计算（Modbus 标准多项式 0xA001）
    static quint16 crc16(const QByteArray& data);
  };