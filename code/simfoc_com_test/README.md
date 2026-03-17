# UART + CAN 通信测试

## 简介
测试两块 STM32F103C8 之间的 UART (USART3) 和 CAN 总线双路通信。按下按键随机发送预设消息，对端 OLED 实时显示接收内容，用于验证：
- USART3 串口收发是否正常
- CAN 总线通信是否正常 (500kbps)
- TJA1050 CAN 收发器是否工作

## 硬件需求
| 器件 | 数量 | 说明 |
|------|------|------|
| STM32F103C8T6 板 | ×2 | 一块烧 Master，一块烧 Slave |
| SSD1306 OLED | ×2 | 每块板各一个 |
| TJA1050 CAN 模块 | ×2 | CAN 收发器 (板上已集成则不需要) |
| JLink 调试器 | ×1 | SWD 烧录 |

## 接线

### 板间连接
```
    Board A (Master)                Board B (Slave)
    ┌───────────────┐               ┌───────────────┐
    │               │    UART       │               │
    │  PB10 (TX) ───┼──────×───────┼── PB11 (RX)   │
    │  PB11 (RX) ───┼────×────────┼── PB10 (TX)   │
    │               │               │               │
    │               │    CAN        │               │
    │  TJA1050 CANH─┼───────────────┼─TJA1050 CANH  │
    │  TJA1050 CANL─┼───────────────┼─TJA1050 CANL  │
    │               │               │               │
    │  GND ─────────┼───────────────┼── GND         │
    └───────────────┘               └───────────────┘
```

### 各板自身接线
```
STM32F103C8         SSD1306 OLED
├── PB4 ──────────── SCL    (软件 I2C)
├── PB3 ──────────── SDA    (软件 I2C)
├── 3.3V ─────────── VCC
└── GND ──────────── GND

STM32F103C8         TJA1050
├── PA12 ─────────── TXD    (CAN TX)
├── PA11 ─────────── RXD    (CAN RX)
├── 3.3V ─────────── VCC
└── GND ──────────── GND

STM32F103C8         板载
├── PC13 ─────────── LED    (发送时闪烁)
└── PA15 ─────────── KEY1   (按下发送消息)
```

## 编译烧录

```bash
# 板 A — Master
pio run -e master -t upload

# 板 B — Slave
pio run -e slave -t upload
```

## 使用方法
1. 分别烧录 Master 和 Slave 固件到两块板
2. 按照上图连接 UART 交叉线 + CAN 总线 + 共地
3. 上电后 OLED 显示角色和 CAN 初始化状态
4. 按任一板的 **KEY1**，随机发送一条消息
5. 对端板 OLED 显示收到的 UART 和 CAN 消息

## 预期现象

### OLED 显示
```
MASTER TX:5
TX> Hello!
UART< SimFOC [3]
CAN<  Ping [2]
```

| 行 | 内容 |
|----|------|
| 第1行 | 角色 (MASTER/SLAVE) + 总发送次数 |
| 第2行 | 最近一次发送的消息 |
| 第3行 | UART 通道最近收到的消息 + 累计接收次数 |
| 第4行 | CAN 通道最近收到的消息 + 累计接收次数 |

### 预设消息池
按 KEY1 时从以下消息中随机选取一条发送：

`Hello!` `FOC OK` `Test12` `CAN OK` `Ping` `STM32` `SimFOC` `Motor!`

### CAN 协议
| 方向 | CAN ID | 说明 |
|------|--------|------|
| Master → Slave | `0x10` | Master 发送帧 |
| Slave → Master | `0x20` | Slave 发送帧 |

波特率：500kbps，标准帧 (11-bit ID)

## 故障排查

| 现象 | 可能原因 |
|------|----------|
| UART 有数据，CAN 无数据 | TJA1050 未供电 / CANH-CANL 接反 / 缺终端电阻 |
| CAN 有数据，UART 无数据 | TX/RX 未交叉 / 波特率不匹配 |
| 两路都无数据 | 未共地 / 接线松动 |
| CAN 初始化 FAIL | 检查 PA11/PA12 连接、TJA1050 供电 |

## 依赖库
- [U8g2](https://github.com/olikraus/u8g2) - OLED 驱动 (软件 I2C)
