# CLAUDE.md - AS5600 磁编码器读数测试项目

## 项目概述
测试 AS5600 磁编码器的 I2C 通信，读取角度数据并在 SSD1306 OLED 上实时显示。

## 硬件连接
| 功能 | 引脚 | 接口类型 |
|------|------|----------|
| AS5600 SCL | PB6 | 硬件 I2C1 |
| AS5600 SDA | PB7 | 硬件 I2C1 |
| OLED SCL | PB4 | 软件 I2C |
| OLED SDA | PB3 | 软件 I2C |
| Serial TX | PB10 | USART3 |
| Serial RX | PB11 | USART3 |

## 技术栈
- STM32F103C8T6 + Arduino 框架 (PlatformIO)
- U8g2 库驱动 SSD1306 OLED (128x64, 软件 I2C)
- Wire 库读取 AS5600 (硬件 I2C1)
- JLink SWD 烧录

## 常用命令
```bash
pio run                  # 编译
pio run -t upload        # 烧录
pio device monitor       # 串口监视器 (115200)
```

## 显示内容
- RAW: 原始角度值 (0~4095)
- DEG: 换算后角度 (0.0~360.0°)
- MAG: 磁铁检测状态 (OK / TOO STRONG / TOO WEAK / NO MAGNET)
- AGC: 自动增益值
