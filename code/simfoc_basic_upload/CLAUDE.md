# CLAUDE.md - STM32F103C8 基础烧录测试项目

## 项目概述
最基础的 LED 闪烁测试，用于验证：
1. JLink SWD 烧录是否正常
2. 8MHz 外部晶振 (HSE) 是否工作
3. GPIO 输出是否正常

## 硬件连接
| 功能 | 引脚 | 备注 |
|------|------|------|
| LED | PC13 | 高电平点亮 |
| Serial TX | PB10 | USART3 |
| Serial RX | PB11 | USART3 |

## 验证方法
- LED 以 1s 间隔闪烁 → 烧录成功、晶振正常
- 串口输出 SystemCoreClock = 72 MHz → HSE 8MHz 经 PLL 倍频正常
- 如果闪烁速度明显偏快/偏慢 → 晶振可能未起振，使用了内部 RC

## 常用命令
```bash
pio run                  # 编译
pio run -t upload        # 烧录
pio device monitor       # 串口监视器 (115200)
```
