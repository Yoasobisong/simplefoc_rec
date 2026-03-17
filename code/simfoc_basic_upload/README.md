# STM32F103C8 基础烧录测试 - LED 闪烁

## 简介
最基础的 STM32 测试程序，通过 LED 闪烁和 OLED 显示系统时钟频率，验证以下硬件是否正常工作：
- JLink SWD 烧录通路
- 8MHz 外部晶振 (HSE)
- GPIO 输出
- SSD1306 OLED 显示

## 硬件需求
| 器件 | 说明 |
|------|------|
| STM32F103C8T6 最小系统板 | 外接 8MHz 晶振 |
| SSD1306 OLED 0.96寸 128x64 | I2C 接口 |
| JLink 调试器 | SWD 模式 |

## 接线

```
STM32F103C8         SSD1306 OLED
├── PB4 ──────────── SCL
├── PB3 ──────────── SDA
├── 3.3V ─────────── VCC
└── GND ──────────── GND

STM32F103C8         板载
└── PC13 ─────────── LED (高电平点亮)
```

## 编译烧录

```bash
pio run               # 编译
pio run -t upload      # 烧录
```

## 预期现象

### LED
- PC13 LED 每 **1 秒** 交替亮灭

### OLED 显示
```
 Basic Blink Test
 LED: ON
 Count: 42
 CLK: 72 MHz
```

### 判断晶振状态
| OLED 显示 | 含义 |
|-----------|------|
| `CLK: 72 MHz` | 外部 8MHz 晶振正常 (HSE × 9 PLL) |
| `CLK: 64 MHz` | 晶振未起振，使用内部 RC (HSI/2 × 16 PLL) |

如果 LED 闪烁速度明显不是 1 秒间隔，也说明时钟源有问题。

## 依赖库
- [U8g2](https://github.com/olikraus/u8g2) - OLED 驱动 (软件 I2C)
