# CLAUDE.md - UART + CAN 通信测试项目

## 项目概述
测试两块 STM32F103C8 之间的 UART 和 CAN 通信，按键发送随机消息，OLED 显示收发状态。

## 硬件连接

### 板间连接
```
   Board A (Master)          Board B (Slave)
   ┌──────────────┐          ┌──────────────┐
   │ PB10 (TX) ───┼────×─────┼─ PB11 (RX)   │  UART 交叉
   │ PB11 (RX) ───┼──×──────┼─ PB10 (TX)   │
   │              │          │              │
   │ PA12 (CANH)──┼──────────┼─ PA12 (CANH) │  CAN 总线
   │ PA11 (CANL)──┼──────────┼─ PA11 (CANL) │  (经 TJA1050)
   │              │          │              │
   │ GND ─────────┼──────────┼─ GND         │  共地
   └──────────────┘          └──────────────┘
```

### 各板自身连接
| 功能 | 引脚 | 备注 |
|------|------|------|
| LED | PC13 | 发送时闪烁 |
| KEY1 | PA15 | 按下发送消息 |
| OLED SCL | PB4 | 软件 I2C |
| OLED SDA | PB3 | 软件 I2C |

## 编译烧录
```bash
# 板 A (Master)
pio run -e master -t upload

# 板 B (Slave)
pio run -e slave -t upload
```

## CAN 协议
| 方向 | CAN ID | 说明 |
|------|--------|------|
| Master → Slave | 0x10 | Master 发送 |
| Slave → Master | 0x20 | Slave 发送 |

## 使用方法
1. 分别烧录 master 和 slave 固件到两块板
2. 连接 UART 交叉线 + CAN 总线 + 共地
3. 按任一板的 KEY1，该板随机发送一条消息
4. 对端 OLED 显示收到的 UART 和 CAN 消息
5. 两路都能收到 → 通信正常

## OLED 显示格式
```
MASTER TX:5
TX> Hello!
UART< SimFOC [3]
CAN<  Ping [2]
```
