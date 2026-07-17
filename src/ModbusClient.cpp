#include "ModbusClient.h"

ModbusClient::ModbusClient(QObject* parent)
{

}

ModbusClient::~ModbusClient()
{

}

QByteArray ModbusClient::buildReadRequest(int slaveId, int funcCode , int startAddr , int count)
{
    QByteArray request;
    request.append(static_cast<char>(slaveId));
    request.append(static_cast<char>(funcCode));
    request.append(static_cast<char>((startAddr>>8)& 0xFF));
    request.append(static_cast<char>(startAddr & 0xFF));
    request.append(static_cast<char>((count>>8)& 0xFF));
    request.append(static_cast<char>((count)& 0xFF));
    quint16 crc = crc16(request);
    request.append(static_cast<char>((crc)&0xFF));
    request.append(static_cast<char>((crc>>8)&0xFF));
    return request;
}

QByteArray ModbusClient::buildWriteSingleRequest(int slaveId, int addr, quint16 value)
{
    QByteArray request;
    request.append(static_cast<char>(slaveId));
    request.append(static_cast<char>(0x06));
    request.append(static_cast<char>((addr>>8)& 0xFF));
    request.append(static_cast<char>(addr & 0xFF));
    request.append(static_cast<char>((value>>8)& 0xFF));
    request.append(static_cast<char>((value)& 0xFF));
    quint16 crc = crc16(request);
    request.append(static_cast<char>((crc)&0xFF));
    request.append(static_cast<char>((crc>>8)&0xFF));
    return request;
}

QByteArray ModbusClient::buildWriteMultiRequest(int slaveId, int funcCode, int startAddr, const QList<quint16>& values)
{
    QByteArray request;
    request.append(static_cast<char>(slaveId));
    request.append(static_cast<char>(funcCode));
    request.append(static_cast<char>((startAddr>>8)& 0xFF));
    request.append(static_cast<char>(startAddr & 0xFF));
    request.append(static_cast<char>((values.size() >> 8) & 0xFF));  // 数量高字节
    request.append(static_cast<char>(values.size() & 0xFF));          // 数量低字节
    request.append(static_cast<char>(values.size() * 2));             // 数据字节数
    for (quint16 value : values)
    {
        request.append(static_cast<char>((value>>8)& 0xFF));
        request.append(static_cast<char>((value)& 0xFF));
    } 
    quint16 crc = crc16(request);
    request.append(static_cast<char>((crc)&0xFF));
    request.append(static_cast<char>((crc>>8)&0xFF));
    return request;
}

Response ModbusClient::parseResponse(const QByteArray& raw)
{
    Response response;
    response.valid = false;

    // 1. 最小长度检查
    if (raw.size() < 5) {
        response.errorString = "帧太短";
        return response;
    }

    // 2. 提取基本信息
    response.slaveId = static_cast<quint8>(raw[0]);
    response.funcCode = static_cast<quint8>(raw[1]);

    // 3. CRC校验
    QByteArray dataPart = raw.left(raw.size() - 2);
    quint16 receivedCrc = static_cast<quint8>(raw[raw.size() - 2]) |
                           (static_cast<quint8>(raw[raw.size() - 1]) << 8);
    quint16 calculatedCrc = crc16(dataPart);

    if (receivedCrc != calculatedCrc) {
        response.errorString = "CRC校验失败";
        return response;
    }

    // 4. 判断是异常响应还是正常响应
    if (response.funcCode >= 0x80) {
        response.errorCode = static_cast<quint8>(raw[2]);
        // 异常码含义
        switch (response.errorCode) {
            case 1: response.errorString = "非法功能码"; break;
            case 2: response.errorString = "非法数据地址"; break;
            case 3: response.errorString = "非法数据值"; break;
            case 4: response.errorString = "从站故障"; break;
            default: response.errorString = QString("未知异常码: %1").arg(response.errorCode); break;
        }
        response.valid = true;  // 帧本身是有效的，只是业务上异常
        return response;
    }

    // ========== 正常响应 ==========
    // 根据功能码判断类型
    if(response.funcCode == 1 || response.funcCode == 2)
    {
        int byteCount = static_cast<quint8>(raw[2]);
        for(int i = 3 ; i < 3 + byteCount ; i++)
        {
            quint8 byte = raw[i];
            for(int j = 0 ; j < 8 ; j++ )
            {
                response.values.append((byte>>j)&0x01);
            }
        }
    }
    else if(response.funcCode == 3 || response.funcCode == 4)
    {
        int byteCount = static_cast<quint8>(raw[2]);
        for(int i = 3 ; i < 3 + byteCount ; i+=2)
        {
            quint16 value = (static_cast<quint8>(raw[i]) << 8)|static_cast<quint8>(raw[i+1]);
            response.values.append(value);
        }
    }
    else if(response.funcCode == 6 || response.funcCode == 16)
    {
        quint16 addr = (static_cast<quint8>(raw[2]) << 8 )| static_cast<quint8>(raw[3]);
        quint16 value = (static_cast<quint8>(raw[4]) << 8) | static_cast<quint8>(raw[5]);
        response.values.append(addr);
        response.values.append(value);
    }
    else
    {
        response.errorString = "不支持的功能码";
        return response;
    }


    response.valid = true;
    return response;
}

QByteArray ModbusClient::wrapMbapHeader(const QByteArray& rtuFrame, quint16 transactionId)
{
    // RTU帧: [从站ID 1B] [功能码 1B] [数据 NB] [CRC 2B]
    // TCP帧: [TxID 2B] [ProtocolID 2B] [Length 2B] [从站ID 1B] [功能码 1B] [数据 NB]
    
    QByteArray pdu = rtuFrame.left(rtuFrame.size() - 2);  // 去掉CRC
    quint16 length = pdu.size();  // 从Unit ID到末尾的字节数

    QByteArray header;
    header.append(static_cast<char>((transactionId >> 8) & 0xFF));  // TxID高字节
    header.append(static_cast<char>(transactionId & 0xFF));          // TxID低字节
    header.append(static_cast<char>(0x00));  // Protocol ID高字节
    header.append(static_cast<char>(0x00));  // Protocol ID低字节
    header.append(static_cast<char>((length >> 8) & 0xFF));  // Length高字节
    header.append(static_cast<char>(length & 0xFF));          // Length低字节

    return header + pdu;
}

QByteArray ModbusClient::unwrapMbapHeader(const QByteArray& tcpFrame, quint16& transactionId)
{
    if (tcpFrame.size() < 7) return QByteArray();

    // 提取Transaction ID
    transactionId = (static_cast<quint8>(tcpFrame[0]) << 8) | static_cast<quint8>(tcpFrame[1]);

    // 去掉前7字节MBAP头，返回PDU（Unit ID + 功能码 + 数据）
    return tcpFrame.mid(7);
}

Response ModbusClient::parseTcpResponse(const QByteArray& raw)
{
    // TCP响应没有CRC，直接解析 [UnitID] [FuncCode] [Data...]
    Response response;
    response.valid = false;

    if (raw.size() < 3) {
        response.errorString = "TCP帧太短";
        return response;
    }

    response.slaveId = static_cast<quint8>(raw[0]);
    response.funcCode = static_cast<quint8>(raw[1]);

    // 异常响应
    if (response.funcCode >= 0x80) {
        response.errorCode = static_cast<quint8>(raw[2]);
        switch (response.errorCode) {
            case 1: response.errorString = "非法功能码"; break;
            case 2: response.errorString = "非法数据地址"; break;
            case 3: response.errorString = "非法数据值"; break;
            case 4: response.errorString = "从站故障"; break;
            default: response.errorString = QString("未知异常码: %1").arg(response.errorCode); break;
        }
        response.valid = true;
        return response;
    }

    // 正常响应
    if (response.funcCode == 1 || response.funcCode == 2) {
        int byteCount = static_cast<quint8>(raw[2]);
        for (int i = 3; i < 3 + byteCount; i++) {
            quint8 byte = raw[i];
            for (int j = 0; j < 8; j++) {
                response.values.append((byte >> j) & 0x01);
            }
        }
    } else if (response.funcCode == 3 || response.funcCode == 4) {
        int byteCount = static_cast<quint8>(raw[2]);
        for (int i = 3; i < 3 + byteCount; i += 2) {
            quint16 value = (static_cast<quint8>(raw[i]) << 8) | static_cast<quint8>(raw[i+1]);
            response.values.append(value);
        }
    } else if (response.funcCode == 6 || response.funcCode == 16) {
        quint16 addr = (static_cast<quint8>(raw[2]) << 8) | static_cast<quint8>(raw[3]);
        quint16 value = (static_cast<quint8>(raw[4]) << 8) | static_cast<quint8>(raw[5]);
        response.values.append(addr);
        response.values.append(value);
    } else {
        response.errorString = "不支持的功能码";
        return response;
    }

    response.valid = true;
    return response;
}

static const quint16 crcTable[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

quint16 ModbusClient::crc16(const QByteArray& data)
{
    quint16 crc = 0xFFFF;  // 初始值
    
    for (int i = 0; i < data.size(); ++i) {
        // 异或字节
        crc ^= static_cast<quint8>(data[i]);
        
        // 查表代替循环8次
        quint8 index = crc & 0xFF;              // 取CRC低8位作为索引
        crc = (crc >> 8) ^ crcTable[index];      // 查表并与CRC高8位组合
    }
    
    return crc;
}