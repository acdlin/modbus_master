#include "RegisterModel.h"
#include <QColor>

RegisterModel::RegisterModel(QObject *parent):QAbstractTableModel(parent)
{

}

int RegisterModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid())
    {
        return 0;
    }
    return m_registers.size();
} 

int RegisterModel::columnCount(const QModelIndex& parent) const
{
    if(parent.isValid())
    {
        return 0;
    }
    return 4;
}



QVariant RegisterModel::data(const QModelIndex &index, int role) const 
{
    if(!index.isValid())
        return QVariant();

    const RegisterEntry &entry = m_registers[index.row()];

    // 背景色：错误行变红
    if (role == Qt::BackgroundRole && entry.hasError)
        return QColor(255, 200, 200);

    // 悬停提示：显示错误信息
    if (role == Qt::ToolTipRole && entry.hasError)
        return entry.errorString;

    // 显示文本
    if (role == Qt::DisplayRole) {
        switch(index.column()){
            case 0: return entry.address;
            case 1: return entry.funcCode;
            case 2: 
                if (entry.funcCode == 1 || entry.funcCode == 2)
                    return entry.value ? "ON" : "OFF";
                return entry.value;
            case 3: return QString("0x%1").arg(entry.value, 4, 16, QChar('0'));
        }
    }

    return QVariant();
}

QVariant RegisterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case 0: return "地址";
            case 1: return "功能码";
            case 2: return "当前值";
            case 3: return "原始Hex";
        }
    }
    return QVariant();  // 垂直表头不处理，Qt 会自动显示行号
}

Qt::ItemFlags RegisterModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (index.column() == 2)
        f |= Qt::ItemIsEditable;
    return f;
}

bool RegisterModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::EditRole)
        return false;

    if (index.column() == 2) {  // 只有"当前值"列可编辑
        m_registers[index.row()].value = value.toUInt();
        emit dataChanged(index, index, {role});  // 通知 View 刷新
        emit registerChanged(index.row() ,value.toUInt());
        return true;
    }
    return false;
}

void RegisterModel::addRegister(int addr, int funcCode)
{
    beginInsertRows(QModelIndex(), m_registers.size(), m_registers.size());
    m_registers.append({addr, funcCode, 0});  // value 默认 0
    endInsertRows();
}

void RegisterModel::updateFromResponse(int slaveId, int funcCode, int startAddr, const QVector<quint16>& values)
{
    // 遍历所有行
    for (int i = 0; i < m_registers.size(); ++i) {
        RegisterEntry &entry = m_registers[i];
        // 功能码匹配 + 地址在响应范围内
        if (entry.funcCode == funcCode && entry.address >= startAddr && entry.address < startAddr + values.size())
        {
            entry.value = values[entry.address - startAddr];
            // 通知 View 这一行的数据变了
            QModelIndex topLeft = index(i, 0);
            QModelIndex bottomRight = index(i, 3);
            emit dataChanged(topLeft, bottomRight);
        }
    }
}


bool RegisterModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid())
        return false;
    if (row < 0 || row + count > m_registers.size())
        return false;

    beginRemoveRows(parent, row, row + count - 1);
    m_registers.remove(row, count);
    endRemoveRows();
    return true;
}

int RegisterModel::address(int row) const
{
    if (row < 0 || row >= m_registers.size())
        return -1;
    return m_registers[row].address;
}

int RegisterModel::funcCode(int row) const
{
    if (row < 0 || row >= m_registers.size())
        return -1;
    return m_registers[row].funcCode;
}

void RegisterModel::setError(int row, const QString& error)
{
    if (row < 0 || row >= m_registers.size()) return;
    m_registers[row].hasError = true;
    m_registers[row].errorString = error;
    emit dataChanged(index(row, 0), index(row, 3));
}

void RegisterModel::clearError(int row)
{
    if (row < 0 || row >= m_registers.size()) return;
    m_registers[row].hasError = false;
    m_registers[row].errorString.clear();
    emit dataChanged(index(row, 0), index(row, 3));
}