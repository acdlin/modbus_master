# Modbus Master

基于 Qt6 的 Modbus 主站调试工具，支持 RTU（串口）和 TCP 两种模式。

## 功能

- **RTU 模式** — 串口通信，支持自定义波特率/数据位/停止位/校验位
- **TCP 模式** — TCP 通信，MBAP 头自动封装/解析
- **功能码支持** — FC01(读线圈)、FC02(读离散输入)、FC03(读保持寄存器)、FC04(读输入寄存器)、FC06(写单个寄存器)、FC16(写多个寄存器)
- **帧监视器** — 实时显示收发帧，TX 蓝色、RX 绿色、错误红色
- **错误处理** — 串口不存在/权限不足/超时/CRC校验失败/从站异常码，均有明确提示
- **线圈显示** — FC01/02 自动显示 ON/OFF
- **轮询** — 可配置间隔的自动轮询
- **右键操作** — 刷新、写单个、写多个、删除

## 依赖

- Qt6 (Widgets, SerialPort, Network)
- CMake 3.16+
- C++17

## 编译

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 运行

```bash
./modbus_master
```

串口权限（Linux）：

```bash
sudo usermod -aG dialout $USER
# 重新登录后生效
```

## 测试脚本

### RTU 从站模拟器

需要虚拟串口：

```bash
sudo apt install socat
socat -d -d pty,raw,echo=0,link=/dev/ttyV0 pty,raw,echo=0,link=/dev/ttyV1
```

运行模拟器：

```bash
python3 scripts/modbus_slave_sim.py
```

主站连接 `/dev/ttyV0`，模拟器监听 `/dev/ttyV1`。

### TCP 从站模拟器

```bash
# 默认监听 502 端口（需要 root）
sudo python3 scripts/modbus_tcp_slave_sim.py

# 或使用非特权端口
python3 scripts/modbus_tcp_slave_sim.py  # 修改脚本中 port 为 1502
```

## 项目结构

```
src/
├── main.cpp              # 入口
├── MainWindow.h/cpp      # 主窗口 UI 与交互
├── ModbusClient.h/cpp    # Modbus 协议层（帧构建/解析/CRC/MBAP头）
├── RegisterModel.h/cpp   # 寄存器表格数据模型
└── ConnectionManager.h/cpp # 连接管理（串口/TCP 收发）
scripts/
├── modbus_slave_sim.py       # RTU 从站模拟器
└── modbus_tcp_slave_sim.py   # TCP 从站模拟器
```
