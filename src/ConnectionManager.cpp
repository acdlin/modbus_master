#include "ConnectionManager.h"
#include <QSerialPortInfo>

ConnectionManager::ConnectionManager(QObject* parent):QObject(parent)
,m_serial(new QSerialPort(this)),m_socket(new QTcpSocket(this)),m_mode(RTU)
{

}

ConnectionManager::~ConnectionManager()
{
}

void ConnectionManager::setMode(Mode mode)
{
    m_mode = mode;
}

void ConnectionManager::setSerialConfig(const QString& port, int baud, int dataBits, int stopBits, int parity)
{
    m_serial->setPortName(port);
    m_serial->setBaudRate(baud);
    m_dataBits = dataBits;
    m_stopBits = stopBits;
    m_parity = parity;
}

void ConnectionManager::setTcpConfig(const QString& host , int port)
{
    m_tcpHost = host;
    m_tcpPort = port;
}

void ConnectionManager::setModbusClient(ModbusClient* client) 
{
    m_modbusClient = client; 
}

void ConnectionManager::connectToDevice()
{
    if(m_mode == RTU)
    {
        // 检查串口是否存在
        bool portExists = false;
        for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
            if (info.portName() == m_serial->portName()) {
                portExists = true;
                break;
            }
        }
        if (!portExists) {
            emit error(QString("串口 %1 不存在，请检查设备是否已连接").arg(m_serial->portName()), ConnectionError);
            return;
        }

        // 设置数据位
        switch (m_dataBits) {
            case 5: m_serial->setDataBits(QSerialPort::Data5); break;
            case 6: m_serial->setDataBits(QSerialPort::Data6); break;
            case 7: m_serial->setDataBits(QSerialPort::Data7); break;
            case 8: default: m_serial->setDataBits(QSerialPort::Data8); break;
        }
        // 设置停止位
        switch (m_stopBits) {
            case 1: m_serial->setStopBits(QSerialPort::OneStop); break;
            case 2: m_serial->setStopBits(QSerialPort::TwoStop); break;
            default: m_serial->setStopBits(QSerialPort::OneStop); break;
        }
        // 设置校验位
        switch (m_parity) {
            case 1: m_serial->setParity(QSerialPort::EvenParity); break;
            case 2: m_serial->setParity(QSerialPort::OddParity); break;
            case 0: default: m_serial->setParity(QSerialPort::NoParity); break;
        }

        if(m_serial->open(QIODevice::ReadWrite))
        {
            emit connected();
        }
        else
        {
            QString err = m_serial->errorString();
            if (m_serial->error() == QSerialPort::PermissionError) {
                err = QString("串口 %1 权限不足，请执行: sudo usermod -aG dialout $USER 然后重新登录").arg(m_serial->portName());
            } else if (m_serial->error() == QSerialPort::DeviceNotFoundError) {
                err = QString("串口 %1 不存在或已被移除").arg(m_serial->portName());
            }
            emit error(err, ConnectionError);
        }
    }
    else
    {
        m_socket->connectToHost(m_tcpHost , m_tcpPort);
        if(m_socket->waitForConnected(3000))
        {
            emit connected();
        }
        else
        {
            QString err;
            switch (m_socket->error()) {
                case QAbstractSocket::ConnectionRefusedError:
                    err = QString("TCP连接被拒绝: 请检查 %1:%2 是否有Modbus从站运行").arg(m_tcpHost).arg(m_tcpPort);
                    break;
                case QAbstractSocket::HostNotFoundError:
                    err = QString("主机 %1 无法找到，请检查IP地址").arg(m_tcpHost);
                    break;
                case QAbstractSocket::SocketTimeoutError:
                    err = QString("连接 %1:%2 超时，请检查网络和从站状态").arg(m_tcpHost).arg(m_tcpPort);
                    break;
                default:
                    err = QString("TCP连接失败: %1").arg(m_socket->errorString());
                    break;
            }
            emit error(err, ConnectionError);
        }
    }
}

void ConnectionManager::disconnectFromDevice()
{
    if (m_mode == RTU)
    {
        m_serial->close();
    }
    else
    {
        m_socket->disconnectFromHost();
    }
    emit disconnected();
}

QByteArray ConnectionManager::sendAndWait(const QByteArray& request, int timeoutMs)
{
    if (m_mode == RTU)
    {
        if (!m_serial->isOpen()) {
            emit error("串口未打开，请先连接", ConnectionError);
            return QByteArray();
        }

        m_serial->write(request);
        if (!m_serial->waitForBytesWritten(timeoutMs)) {
            emit error("串口写入超时", TimeoutError);
            emit frameError("TX写入超时", TimeoutError);
            return QByteArray();
        }
        emit frameSent(request);

        if (m_serial->waitForReadyRead(timeoutMs))
        {
            QByteArray response = m_serial->readAll();
            while (m_serial->waitForReadyRead(50)) {
                response += m_serial->readAll();
            }
            emit frameReceived(response);
            return response;
        }
        else
        {
            emit error("响应超时：从站无响应", TimeoutError);
            emit frameError("RX超时：从站无响应", TimeoutError);
            return QByteArray();
        }
    }
    else
    {
        if (m_socket->state() != QAbstractSocket::ConnectedState) {
            emit error("TCP未连接，请先连接", ConnectionError);
            return QByteArray();
        }

        // TCP模式：给RTU帧加MBAP头
        m_transactionId++;
        QByteArray tcpRequest = m_modbusClient->wrapMbapHeader(request, m_transactionId);
        
        m_socket->write(tcpRequest);
        if (!m_socket->waitForBytesWritten(timeoutMs)) {
            emit error("TCP写入超时", TimeoutError);
            emit frameError("TX写入超时", TimeoutError);
            return QByteArray();
        }
        emit frameSent(tcpRequest);  // 显示TCP帧（带MBAP头）

        if (m_socket->waitForReadyRead(timeoutMs))
        {
            QByteArray response = m_socket->readAll();
            while (m_socket->waitForReadyRead(50)) {
                response += m_socket->readAll();
            }
            emit frameReceived(response);  // 显示TCP帧（带MBAP头）

            // 去掉MBAP头，返回PDU给调用者解析
            quint16 rxTxId;
            QByteArray pdu = m_modbusClient->unwrapMbapHeader(response, rxTxId);
            if (pdu.isEmpty()) {
                emit error("TCP响应MBAP头无效", ConnectionError);
                return QByteArray();
            }
            if (rxTxId != m_transactionId) {
                emit error("Transaction ID不匹配", ConnectionError);
                return QByteArray();
            }
            return pdu;
        }
        else
        {
            emit error("TCP响应超时：从站无响应", TimeoutError);
            emit frameError("RX超时：从站无响应", TimeoutError);
            return QByteArray();
        }
    }
}
