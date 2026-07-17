#!/usr/bin/env python3
"""极简 Modbus TCP 从站模拟器 - 监听 127.0.0.1:502"""

import socket
import struct

# 模拟寄存器: 地址 0~9
registers = [100, 200, 300, 400, 500, 600, 700, 800, 900, 1000]

def handle_read_holding(unit_id, data):
    """功能码 03: 读保持寄存器"""
    start_addr = (data[2] << 8) | data[3]
    count = (data[4] << 8) | data[5]

    if start_addr + count > len(registers):
        return bytes([unit_id, 0x83, 0x02])  # 异常码02: 非法地址

    byte_count = count * 2
    resp = bytes([unit_id, 0x03, byte_count])
    for i in range(count):
        val = registers[start_addr + i]
        resp += bytes([(val >> 8) & 0xFF, val & 0xFF])
    return resp

def handle_read_input(unit_id, data):
    """功能码 04: 读输入寄存器"""
    start_addr = (data[2] << 8) | data[3]
    count = (data[4] << 8) | data[5]

    if start_addr + count > len(registers):
        return bytes([unit_id, 0x84, 0x02])

    byte_count = count * 2
    resp = bytes([unit_id, 0x04, byte_count])
    for i in range(count):
        val = registers[start_addr + i]
        resp += bytes([(val >> 8) & 0xFF, val & 0xFF])
    return resp

def handle_read_coils(unit_id, data):
    """功能码 01: 读线圈"""
    start_addr = (data[2] << 8) | data[3]
    count = (data[4] << 8) | data[5]

    if start_addr + count > len(registers) * 16:
        return bytes([unit_id, 0x81, 0x02])

    byte_count = (count + 7) // 8
    resp = bytes([unit_id, 0x01, byte_count])
    for b in range(byte_count):
        byte_val = 0
        for bit in range(8):
            idx = start_addr + b * 8 + bit
            if idx < len(registers) and registers[idx] != 0:
                byte_val |= (1 << bit)
        resp += bytes([byte_val])
    return resp

def handle_read_discrete(unit_id, data):
    """功能码 02: 读离散输入"""
    start_addr = (data[2] << 8) | data[3]
    count = (data[4] << 8) | data[5]

    if start_addr + count > len(registers) * 16:
        return bytes([unit_id, 0x82, 0x02])

    byte_count = (count + 7) // 8
    resp = bytes([unit_id, 0x02, byte_count])
    for b in range(byte_count):
        byte_val = 0
        for bit in range(8):
            idx = start_addr + b * 8 + bit
            if idx < len(registers) and registers[idx] != 0:
                byte_val |= (1 << bit)
        resp += bytes([byte_val])
    return resp

def handle_write_single(unit_id, data):
    """功能码 06: 写单个寄存器"""
    addr = (data[2] << 8) | data[3]
    value = (data[4] << 8) | data[5]

    if addr >= len(registers):
        return bytes([unit_id, 0x86, 0x02])

    registers[addr] = value
    print(f"  写入: 寄存器[{addr}] = {value}")
    return bytes([unit_id, 0x06, data[2], data[3], data[4], data[5]])

def handle_write_multi(unit_id, data):
    """功能码 16: 写多个寄存器"""
    start_addr = (data[2] << 8) | data[3]
    count = (data[4] << 8) | data[5]
    byte_count = data[6]

    if start_addr + count > len(registers):
        return bytes([unit_id, 0x90, 0x02])

    for i in range(count):
        val = (data[7 + i*2] << 8) | data[7 + i*2 + 1]
        registers[start_addr + i] = val
        print(f"  写入: 寄存器[{start_addr + i}] = {val}")

    resp = bytes([unit_id, 0x10, data[2], data[3], data[4], data[5]])
    return resp

def main():
    host = '127.0.0.1'
    port = 502

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind((host, port))
    server.listen(1)

    print(f"Modbus TCP 从站模拟器启动，监听 {host}:{port}")
    print(f"模拟寄存器: {registers}")
    print("注意: 需要root权限绑定502端口，或改用其他端口(如1502)")

    while True:
        conn, addr = server.accept()
        print(f"\n客户端连接: {addr}")

        try:
            while True:
                # 先读MBAP头(7字节)
                header = conn.recv(7)
                if len(header) < 7:
                    break

                # 解析MBAP头
                tx_id = (header[0] << 8) | header[1]
                protocol_id = (header[2] << 8) | header[3]
                length = (header[4] << 8) | header[5]
                unit_id = header[6]

                # 读取PDU部分
                pdu = conn.recv(length - 1)  # length包含unit_id的1字节，已读
                if len(pdu) < length - 1:
                    break

                func_code = pdu[0]
                print(f"收到请求: TxID={tx_id} UnitID={unit_id} FuncCode=0x{func_code:02X}")

                # 处理请求
                if func_code == 0x03:
                    resp_pdu = handle_read_holding(unit_id, pdu)
                elif func_code == 0x04:
                    resp_pdu = handle_read_input(unit_id, pdu)
                elif func_code == 0x01:
                    resp_pdu = handle_read_coils(unit_id, pdu)
                elif func_code == 0x02:
                    resp_pdu = handle_read_discrete(unit_id, pdu)
                elif func_code == 0x06:
                    resp_pdu = handle_write_single(unit_id, pdu)
                elif func_code == 0x10:
                    resp_pdu = handle_write_multi(unit_id, pdu)
                else:
                    resp_pdu = bytes([unit_id, func_code | 0x80, 0x01])

                # 构建TCP响应: MBAP头 + PDU
                resp_length = len(resp_pdu) + 1  # +1 for unit_id
                mbap = struct.pack('>HHHB', tx_id, protocol_id, resp_length, unit_id)
                response = mbap + resp_pdu

                conn.send(response)
                print(f"发送响应: {response.hex()}")
        except ConnectionResetError:
            print("客户端断开连接")
        finally:
            conn.close()

if __name__ == '__main__':
    main()
