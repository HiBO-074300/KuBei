# KuBei（酷贝）手持终端 — 整机验证工程

本工程是 **KuBei（酷贝，Counect）手持终端整机** 的硬件功能验证实例工程（点灯/点亮/自检 Demo），
基于 **STM32CubeMX + HAL 库 + Keil MDK-ARM**，用于验证整机各外设（LCD、按键、RTC、Flash、
电源管理、ADC 电压采集等）是否工作正常。

## 实物照片

照片位于本目录 `Img/` 文件夹：

| 照片 | 说明 |
|---|---|
| `Img/正面.jpg` | 整机正面：蓝色外壳 + 2.4" 彩屏 + 20 键矩阵键盘（品牌 Counect 酷贝） |
| `Img/全家福.jpg` | 整机拆解全家福：主板 + 屏幕 + 3.7V/1000mAh 锂电池 + 键盘按键 |
| `Img/主板布局.jpg` | 主板布局：STM32F103 主控、TM1668 按键芯片、电源电路等（板号 CUPE_REV F1.1） |
| `Img/屏幕型号.jpg` / `Img/屏幕参数.png` | 屏幕背面丝印：YIHUA YH-024CK3603V2，36pin FPC 排线 |
| `Img/安装示意.jpg` | 主板与蓝色外壳的安装示意 |

## 硬件资源

| 模块 | 器件 / 引脚 | 说明 |
|---|---|---|
| MCU | STM32F103VET6（LQFP100），HSE + PLL×9 = **72MHz** | CubeMX 工程 `KuBei_Test.ioc` |
| 显示屏 | ILI9327 彩色 TFT，原生 240×320，本机横屏 320×240 | FSMC Bank1 **NE1(PD7)**，16 位并口，RS = A16(PD11)；命令 `0x6000_0000` / 数据 `0x6002_0000`；复位 PC9，背光 PC8（TIM3_CH3 PWM，完全重映射） |
| 矩阵键盘 | TM1668，K1/K2 × SEG1~10 = **20 键** | 软件三线：STB=PE2 / CLK=PE3 / DIO=PE4 |
| RTC | PCF8563 | I2C1，地址 0x51；首次上电写基线时间，掉电后自动恢复 |
| 数据 Flash | W25Q64（8MB） | SPI1（PA5/6/7），片选 PA4；上电做擦/写/读自检 |
| 字库 Flash | SPI 字库芯片（GB2312 汉字字模） | SPI2，片选 PB12；提供 12x24 / 24x24 等字体 |
| 电源管理 | 软件自锁开关 | `POW_SOFT_EN` = PE0 维持供电；开机键 = PE1（内部下拉）；**长按 3 秒软关机** |
| 电压采集 | ADC1_IN10/IN11 | PC0 = 充电/电源、PC1 = 电池，均为 33K+33K 分压（实际电压 = ADC 电压 × 2） |
| 调试串口 | USART1 (PA9)，115200-8-N-1 | 输出各模块初始化结果与运行日志 |
| 其它外设 | USART3、USB Device、I2C2、SPI3、BEEP(PB1)、MF522/M24 预留信号 | 本 Demo 暂未启用业务逻辑 |

## 软件架构

```
应用层          main.c（UI 界面 / 业务轮询）      gui.c（图形库：纯逻辑，与硬件解耦）
                   ↓ GUI_DispDrv_t / GUI_Font_t
显示驱动层      disp_ili9327.c（ILI9327 + FSMC）  disp_font_flash.c（SPI2 字库 → 字体接口）
                   ↓
硬件层          lcd_ili9327.c（FSMC 读写）        font_lib.c（字库 Flash 驱动）
外设驱动        power_ctrl.c（开关机）  power_adc.c（电压）  tm1668.c（键盘）  rtc_pcf8563.c  w25q64.c
```

★ 硬件相关配置全部集中在各 `*_cfg.h` 中，换屏/换引脚只改配置文件：
`lcd_ili9327_cfg.h`（屏幕引脚/时序/方向）、`tm1668_cfg.h`（键盘三线时序）、`rtc_pcf8563_cfg.h`（RTC 总线）。

## 当前 Demo 功能（上电即可看到）

1. 软件自锁开机：按 PE1 开机键上电，松开后由 PE0 维持供电；
2. 屏幕显示 RTC 实时时钟（HH:MM:SS 大字，每秒刷新）；
3. 显示电池电压（BAT）/ 充电电压（CHG），500ms 刷新，数值变化才重画；
4. 显示开机键状态（PRESSED/RELEASED）与 TM1668 矩阵键值；
5. 长按开机键 3 秒 → 屏幕提示 POWER OFF 后断电（串口发送小写 `o` 也可关机，用于单独验证关机通路）；
6. 串口打印各模块自检结果（LCD ID、两片 Flash JEDEC ID、ADC、RTC 等）。

## 目录结构

```
KuBei_Test/
├── KuBei_Test.ioc        # STM32CubeMX 工程配置
├── Core/
│   ├── Inc/  Src/        # 应用代码（main.c 及各外设驱动）
│   ├── Fontlib.c/h       # 内置字库数据
│   └── LCD_FSMC_配置与初始化说明.md   # FSMC 原理笔记（旧工程 ST7565，仅供参考）
├── Drivers/              # STM32 HAL 库与 CMSIS
├── MDK-ARM/              # Keil MDK 工程文件
└── ../Img/               # 整机实物照片
```

## 编译与烧录

1. 用 **Keil MDK-ARM** 打开 `MDK-ARM/` 下的工程，编译后烧录到 STM32F103VET6；
2. 需要改外设配置时，用 **STM32CubeMX** 打开 `KuBei_Test.ioc` 重新生成
   （注意：`POWER_SelfHoldEarly()` 必须保持在 `main()` 第一行，PE0 初始电平需设为 High）。

## 已知注意事项

- ILI9327 的 GRAM 原生为 240×432，未写入的行会显示黑白条纹，驱动中 `ClearPanel()` 已整片刷白处理；
- ADC 分压后源阻抗约 16.5K，采样时间已放宽到 239.5 周期，CubeMX 默认 1.5 周期会读数偏低；
- `Img/正面.jpg` 文件名中含有一个不可见字符（零宽空格），链接失效时请重命名后修正路径。
