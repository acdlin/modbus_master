#include "MainWindow.h"
#include <QTableView>
#include <QDockWidget>
#include <QHeaderView>
#include <QFormLayout>
#include <QLineEdit>
#include <QMenu>
#include <QInputDialog>
#include <QTime>
#include <QTimer>
#include <QSerialPortInfo>
#include <QMessageBox>
#include <QStatusBar>
#include <algorithm>
#include "RegisterModel.h"

MainWindow::MainWindow(QWidget* parent):QMainWindow(parent)
{
    m_connManager = new ConnectionManager(this);
    m_modbusClient = new ModbusClient(this);
    m_connManager->setModbusClient(m_modbusClient);
    setWindowTitle("ModbusMaster");
    resize(900 , 600);
    m_tableView = new QTableView(this);
    m_model = new RegisterModel(this);
    m_tableView->setModel(m_model);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    setCentralWidget(m_tableView);

    m_connectionDock = new QDockWidget("连接配置" , this);
    QWidget *connPanel = new QWidget;
    QFormLayout *form = new QFormLayout(connPanel);  // connPanel 自动接管 layout

    m_modeCombo = new QComboBox;
    m_modeCombo->addItems({"RTU", "TCP"});

    m_slaveIdSpin = new QSpinBox;
    m_slaveIdSpin->setRange(1, 247);
    m_connectBtn = new QPushButton("连接");
    m_statusLabel = new QLabel;
    m_statusLabel->setFixedSize(16, 16);
    m_statusLabel->setStyleSheet("background-color: red; border-radius: 8px;");

    // RTU 控件组
    m_rtuGroup = new QWidget;
    QFormLayout *rtuLayout = new QFormLayout(m_rtuGroup);
    m_portCombo = new QComboBox;
    for(const QSerialPortInfo &info : QSerialPortInfo::availablePorts()){
        m_portCombo->addItem(info.portName());
    }
    m_portCombo->setEditable(true);
    m_baudCombo = new QComboBox;
    m_baudCombo->addItems({"9600", "19200", "38400", "115200"});
    m_dataBitsCombo = new QComboBox;
    m_dataBitsCombo->addItems({"5", "6", "7", "8"});
    m_stopBitsCombo = new QComboBox;
    m_stopBitsCombo->addItems({"1", "1.5", "2"});
    m_parityCombo = new QComboBox;
    m_parityCombo->addItems({"None", "Even", "Odd"});
    rtuLayout->addRow("端口", m_portCombo);
    rtuLayout->addRow("波特率", m_baudCombo);
    rtuLayout->addRow("数据位", m_dataBitsCombo);
    rtuLayout->addRow("停止位", m_stopBitsCombo);
    rtuLayout->addRow("校验位", m_parityCombo);

    // TCP 控件组
    m_tcpGroup = new QWidget;
    QFormLayout *tcpLayout = new QFormLayout(m_tcpGroup);
    m_ipEdit = new QLineEdit("127.0.0.1");
    m_tcpPortSpin = new QSpinBox;
    m_tcpPortSpin->setRange(1, 65535);
    m_tcpPortSpin->setValue(502);
    tcpLayout->addRow("IP", m_ipEdit);
    tcpLayout->addRow("端口", m_tcpPortSpin);

    // 主布局
    form->addRow("模式", m_modeCombo);
    form->addRow("从站ID", m_slaveIdSpin);
    form->addRow(m_rtuGroup);   // RTU 整组加入
    form->addRow(m_tcpGroup);   // TCP 整组加入
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(m_connectBtn);
    btnLayout->addWidget(m_statusLabel);
    btnLayout->addStretch();
    form->addRow(btnLayout);

    // 默认显示 RTU，隐藏 TCP
    m_tcpGroup->hide();

    // 连接按钮
   

    m_connectionDock->setWidget(connPanel);
    connPanel->setMinimumWidth(200);
    addDockWidget(Qt::LeftDockWidgetArea,m_connectionDock);

    m_frameDock = new QDockWidget("原始帧监视器", this);
    m_frameView = new QPlainTextEdit;
    m_frameView->setReadOnly(true);
    m_frameView->setFont(QFont("Monospace", 10));
    m_frameDock->setWidget(m_frameView);
    addDockWidget(Qt::BottomDockWidgetArea, m_frameDock);
    m_pollIntervalSpin = new QSpinBox;
    m_pollIntervalSpin->setRange(100, 10000);
    m_pollIntervalSpin->setValue(1000);
    m_pollIntervalSpin->setSuffix(" ms");
    m_pollBtn = new QPushButton("开始轮询");
    m_pollTimer = new QTimer(this);

    form->addRow("轮询间隔", m_pollIntervalSpin);
    form->addRow(m_pollBtn);
    connect(m_modeCombo , &QComboBox::currentIndexChanged , this , [this](int index)
    {
        if(index == 0)
        {
            m_rtuGroup->show();
            m_tcpGroup->hide();
        }
        else
        {
            m_rtuGroup->hide();
            m_tcpGroup->show();
        }
    });

    // 开始/停止轮询
connect(m_pollBtn, &QPushButton::clicked, this, [this](){
    if (m_pollTimer->isActive()) {
        m_pollTimer->stop();
        m_pollBtn->setText("开始轮询");
    } else {
        m_pollTimer->setInterval(m_pollIntervalSpin->value());
        m_pollIndex = 0;
        m_pollTimer->start();
        m_pollBtn->setText("停止轮询");
    }
});

// 定时器触发：逐行刷新
connect(m_pollTimer, &QTimer::timeout, this, [this](){
    if (m_model->rowCount(QModelIndex()) == 0) return;
    
    int row = m_pollIndex % m_model->rowCount(QModelIndex());
    int addr = m_model->address(row);
    int fc = m_model->funcCode(row);
    int slaveId = m_slaveIdSpin->value();

    QByteArray request = m_modbusClient->buildReadRequest(slaveId, fc, addr, 1);
    QByteArray response = m_connManager->sendAndWait(request, 1000);
    auto result = (m_modeCombo->currentIndex() == 0) ? m_modbusClient->parseResponse(response) : m_modbusClient->parseTcpResponse(response);
    if (response.isEmpty()) {
        m_model->setError(row, "超时：从站无响应");
    } else if (!result.valid) {
        m_model->setError(row, "CRC校验失败");
    } else if (result.funcCode >= 0x80) {
        m_model->setError(row, result.errorString);
    } else {
        m_model->updateFromResponse(slaveId, fc, addr, result.values);
        m_model->clearError(row);
    }

    m_pollIndex++;
});

    connect(m_connectBtn, &QPushButton::clicked, this, [this](){
        if (!m_connected) {
            // 读取 UI 配置，设置到 ConnectionManager
            if (m_modeCombo->currentIndex() == 0) {
                m_connManager->setMode(ConnectionManager::RTU);
                m_connManager->setSerialConfig(
                m_portCombo->currentText(),
                m_baudCombo->currentText().toInt(),
                m_dataBitsCombo->currentText().toInt(),
                m_stopBitsCombo->currentText().toInt(),
                m_parityCombo->currentIndex()
            );
            } else {
                m_connManager->setMode(ConnectionManager::TCP);
                m_connManager->setTcpConfig(m_ipEdit->text(), m_tcpPortSpin->value());
            }
            m_connManager->connectToDevice();
        } else {
            m_connManager->disconnectFromDevice();
        }
    });
    connect(m_connManager , &ConnectionManager::connected, this , [this](){
        m_connected = true;
        m_connectBtn->setText("断开");
        m_statusLabel->setStyleSheet("background-color: green; border-radius: 8px;");
    });
    connect(m_connManager, &ConnectionManager::disconnected, this, [this](){
    m_connected = false;
    m_connectBtn->setText("连接");
    m_statusLabel->setStyleSheet("background-color: red; border-radius: 8px;");
});

    // 连接错误 → QMessageBox
    connect(m_connManager, &ConnectionManager::error, this, [this](const QString& msg, ConnectionManager::ErrorType type){
        if (type == ConnectionManager::ConnectionError) {
            QMessageBox::warning(this, "连接错误", msg);
        }
        statusBar()->showMessage(msg, 5000);
    });

    // 帧监视器：错误帧红色显示
    connect(m_connManager, &ConnectionManager::frameError, this, [this](const QString& msg, ConnectionManager::ErrorType type){
        QString time = QTime::currentTime().toString("[HH:mm:ss.zzz]");
        QString label;
        switch (type) {
            case ConnectionManager::TimeoutError: label = "TIMEOUT"; break;
            case ConnectionManager::CrcError: label = "CRC ERROR"; break;
            case ConnectionManager::ExceptionError: label = "EXCEPTION"; break;
            default: label = "ERROR"; break;
        }
        m_frameView->appendHtml(QString("<span style='color:red;font-weight:bold;'>%1 %2: %3</span>")
            .arg(time, label, msg));
    });

    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tableView, &QTableView::customContextMenuRequested, this, [this](const QPoint &pos){
        QMenu menu;
        QModelIndexList selected = m_tableView->selectionModel()->selectedRows();

        menu.addAction("添加寄存器", this, [this](){
            bool ok;
            int addr = QInputDialog::getInt(this, "添加寄存器", "地址:", 40001, 1, 65535, 1, &ok);
            if (ok) {
                int funcCode = QInputDialog::getInt(this, "添加寄存器", "功能码:", 3, 1, 4, 1, &ok);
                if (ok)
                    m_model->addRegister(addr, funcCode);
            }
        });

        QAction *deleteAction = menu.addAction("删除", this, [this](){
            QModelIndexList sel = m_tableView->selectionModel()->selectedRows();
            if (!sel.isEmpty()) {
                int row = sel.first().row();
                m_model->removeRow(row);
            }
        });

        QAction *refreshAction = menu.addAction("刷新", this, [this](){
            QModelIndexList sel = m_tableView->selectionModel()->selectedRows();
            if(sel.isEmpty())return;
            int row = sel.first().row();
            int addr = m_model->address(row);
            int fc = m_model->funcCode(row);
            int slaveId = m_slaveIdSpin->value();

            QByteArray request = m_modbusClient->buildReadRequest(slaveId , fc , addr , 1);
            QByteArray response = m_connManager->sendAndWait(request, 1000);
            auto result = (m_modeCombo->currentIndex() == 0) ? m_modbusClient->parseResponse(response) : m_modbusClient->parseTcpResponse(response);
            if (response.isEmpty()) {
                m_model->setError(row, "超时：从站无响应");
            } else if (!result.valid) {
                m_model->setError(row, "CRC校验失败");
            } else if (result.funcCode >= 0x80) {
                m_model->setError(row, result.errorString);
            } else {
                m_model->updateFromResponse(slaveId, fc, addr, result.values);
                m_model->clearError(row);
            }
        });

        // 写多个寄存器
        QAction *writeMultiAction = menu.addAction("写多个寄存器(FC16)", this, [this](){
            QModelIndexList sel = m_tableView->selectionModel()->selectedRows();
            if (sel.isEmpty()) return;

            std::sort(sel.begin(), sel.end());
            int slaveId = m_slaveIdSpin->value();
            int startRow = sel.first().row();
            int startAddr = m_model->address(startRow);
            QList<quint16> values;

            for (const QModelIndex &idx : sel) {
                int row = idx.row();
                bool ok;
                int val = QInputDialog::getInt(this, "写多个寄存器",
                    QString("寄存器 %1 的值:").arg(m_model->address(row)),
                    m_model->data(m_model->index(row, 2), Qt::DisplayRole).toUInt(), 0, 65535, 1, &ok);
                if (!ok) return;
                values.append(static_cast<quint16>(val));
            }

            QByteArray request = m_modbusClient->buildWriteMultiRequest(slaveId, 16, startAddr, values);
            QByteArray response = m_connManager->sendAndWait(request, 1000);
            auto result = (m_modeCombo->currentIndex() == 0) ? m_modbusClient->parseResponse(response) : m_modbusClient->parseTcpResponse(response);

            if (response.isEmpty()) {
                QMessageBox::warning(this, "写入失败", "超时：从站无响应");
            } else if (!result.valid) {
                QMessageBox::warning(this, "写入失败", "CRC校验失败");
            } else if (result.funcCode >= 0x80) {
                QMessageBox::warning(this, "写入失败", result.errorString);
            } else {
                int fc = m_model->funcCode(startRow);
                QByteArray readReq = m_modbusClient->buildReadRequest(slaveId, fc, startAddr, values.size());
                QByteArray readResp = m_connManager->sendAndWait(readReq, 1000);
                auto readResult = (m_modeCombo->currentIndex() == 0) ? m_modbusClient->parseResponse(readResp) : m_modbusClient->parseTcpResponse(readResp);
                if (readResult.valid && readResult.funcCode < 0x80) {
                    QVector<quint16> vals;
                    for (quint16 v : readResult.values) vals.append(v);
                    m_model->updateFromResponse(slaveId, fc, startAddr, vals);
                    for (const QModelIndex &idx : sel) {
                        m_model->clearError(idx.row());
                    }
                }
                statusBar()->showMessage("写入成功", 3000);
            }
        });

        menu.addSeparator();
        menu.addAction("清空监视器", this, [this](){
            m_frameView->clear();
        });
        
        deleteAction->setEnabled(!selected.isEmpty());
        refreshAction->setEnabled(!selected.isEmpty());
        writeMultiAction->setEnabled(selected.size() >= 2);

        menu.exec(m_tableView->viewport()->mapToGlobal(pos));
    });
    // TX：发送的帧，蓝色显示
    connect(m_connManager, &ConnectionManager::frameSent, this, [this](const QByteArray& frame){
        QString hex = frame.toHex(' ').toUpper();  // 字节转成 "01 03 00 00" 格式
        QString time = QTime::currentTime().toString("[HH:mm:ss.zzz]");  // 时间戳
        m_frameView->appendHtml(QString("<span style='color:blue;'>%1 TX: %2</span>").arg(time, hex));
    });

    // RX：收到的帧，绿色显示
    connect(m_connManager, &ConnectionManager::frameReceived, this, [this](const QByteArray& frame){
        QString hex = frame.toHex(' ').toUpper();
        QString time = QTime::currentTime().toString("[HH:mm:ss.zzz]");
        m_frameView->appendHtml(QString("<span style='color:green;'>%1 RX: %2</span>").arg(time, hex));
    });
    connect(m_model, &RegisterModel::registerChanged, this, [this](int row, quint16 newValue){
    int addr = m_model->address(row);
    int fc = m_model->funcCode(row);
    int slaveId = m_slaveIdSpin->value();

    QByteArray request = m_modbusClient->buildWriteSingleRequest(slaveId, addr, newValue);
    QByteArray response = m_connManager->sendAndWait(request, 1000);
    auto result = (m_modeCombo->currentIndex() == 0) ? m_modbusClient->parseResponse(response) : m_modbusClient->parseTcpResponse(response);

    if (response.isEmpty()) {
        m_model->setError(row, "写入超时");
    } else if (!result.valid) {
        m_model->setError(row, "写入CRC校验失败");
    } else if (result.funcCode >= 0x80) {
        m_model->setError(row, "写入失败：" + result.errorString);
    } else {
        // 写入成功，读回验证
        QByteArray readReq = m_modbusClient->buildReadRequest(slaveId, fc, addr, 1);
        QByteArray readResp = m_connManager->sendAndWait(readReq, 1000);
        auto readResult = (m_modeCombo->currentIndex() == 0) ? m_modbusClient->parseResponse(readResp) : m_modbusClient->parseTcpResponse(readResp);
        if (readResult.valid && readResult.funcCode < 0x80) {
            m_model->updateFromResponse(slaveId, fc, addr, readResult.values);
            m_model->clearError(row);
        }
    }
});
}


MainWindow::~MainWindow()
{
    
}
