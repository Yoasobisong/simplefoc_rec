# CLAUDE.md - SimpleFOC STM32F103C8 伺服控制项目

## 项目概述
基于 STM32F103C8T6 + SimpleFOC 的无刷电机 FOC 控制项目，实现类舵机的位置闭环控制。

## 硬件配置

### 核心器件
- **主控**: STM32F103C8T6, 8MHz 外部晶振
- **电机驱动**: DRV8313 (集成三半桥)
- **电机**: GM2208 云台无刷电机, 7 极对数, 限位 300 度
- **编码器**: AS5600 磁编码器 (I2C, 绝对位置, 12bit)
- **电源**: 12V 输入 → MP2315 降压至 5V → AMS1117-3.3 降压至 3.3V

### 引脚分配
| 功能 | 引脚 | 备注 |
|------|------|------|
| PWM_U | PA8 | TIM1_CH1 |
| PWM_V | PA9 | TIM1_CH2 |
| PWM_W | PA10 | TIM1_CH3 |
| DRV8313 EN | PB15 | 驱动使能 |
| AS5600 SCL | PB6 | I2C1 |
| AS5600 SDA | PB7 | I2C1 |
| OLED SCL | PB4 | 软件 I2C |
| OLED SDA | PB3 | 软件 I2C |
| Serial TX | PB10 | USART3 |
| Serial RX | PB11 | USART3 |
| CAN TX | PA12 | TJA1050 |
| CAN RX | PA11 | TJA1050 |
| LED | PC13 | 低电平点亮 |
| KEY1 | PA15 | 上拉输入 |
| SWDIO | PA13 | 烧录调试 |
| SWCLK | PA14 | 烧录调试 |

### 重要注意事项
- USART1 (PA9/PA10) 与 PWM 引脚冲突，**必须使用 USART3 (PB10/PB11)**
- 通过 `build_flags = -D SERIAL_UART_INSTANCE=3` 重定向 Serial 到 USART3
- OLED 使用 PB3/PB4, 不是硬件 I2C 引脚, 需要软件 I2C
- AS5600 使用 PB6/PB7 (I2C1 默认引脚)

## 技术栈
- **框架**: Arduino (PlatformIO)
- **FOC 库**: SimpleFOC v2.4.0
- **烧录**: JLink (SWD)

## 常用命令
```bash
pio run                  # 编译
pio run -t upload        # 烧录
pio device monitor       # 串口监视器 (115200)
```

## 调参指南
通过串口发送命令实时调参:
- `T3.14` - 设置目标角度 (弧度)
- `M` - 进入电机配置子菜单 (PID 调参等)
