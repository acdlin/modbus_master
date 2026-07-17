#pragma once
#include <QMainWindow>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QTableView>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include "ConnectionManager.h"
#include "ModbusClient.h"
#include "RegisterModel.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
    public:
        MainWindow(QWidget* parent = nullptr);
        ~MainWindow();

    private:
    QWidget *m_rtuGroup;
    QWidget *m_tcpGroup;
    QDockWidget *m_connectionDock;
    QDockWidget *m_frameDock;
    QPlainTextEdit *m_frameView;
    QTableView *m_tableView;
    RegisterModel *m_model;
    QComboBox *m_modeCombo;
    QSpinBox *m_slaveIdSpin;
    QComboBox *m_portCombo;
    QComboBox *m_baudCombo;
    QComboBox *m_dataBitsCombo;
    QComboBox *m_stopBitsCombo;
    QComboBox *m_parityCombo;
    QLineEdit *m_ipEdit;
    QSpinBox *m_tcpPortSpin;
    QPushButton *m_connectBtn;
    QLabel *m_statusLabel;
    bool m_connected = false;
    ConnectionManager *m_connManager;
    ModbusClient *m_modbusClient;
    QSpinBox *m_pollIntervalSpin;   // 轮询间隔
    QPushButton *m_pollBtn;         // 开始/停止轮询按钮
    QTimer *m_pollTimer;            // 定时器
    int m_pollIndex = 0;            // 当前轮询到第几行

};