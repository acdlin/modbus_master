#pragma once

#include <QAbstractTableModel>
#include <QVector>
#include <QObject>


struct RegisterEntry{
    int address;
    int funcCode;
    quint16 value;
    bool hasError = false;
    QString errorString;
};

class RegisterModel : public QAbstractTableModel
{
    Q_OBJECT
    
public:
    explicit RegisterModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex & parent) const override;
    int columnCount(const QModelIndex& parent) const override;
    QVariant data(const QModelIndex &index , int role) const override;
    QVariant headerData(int section , Qt::Orientation orientation , int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index , const QVariant &value , int role) override;
    void addRegister(int addr , int funcCode);
    void updateFromResponse(int slaveId , int funcCode , int startAddr , const QVector<quint16>& values);
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    int address(int row) const;
    int funcCode(int row) const;
    void setError(int row , const QString& error);
    void clearError(int row);

signals:
    void registerChanged(int row , quint16 newValue);    

private:
    QVector<RegisterEntry> m_registers;
};

