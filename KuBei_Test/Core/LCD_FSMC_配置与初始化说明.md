# STM32F103 驱动 ZYMG12864 单色 LCD —— FSMC 配置与初始化说明

> ?? 下文是**旧工程(STM32_Calendar / ST7565 单色 12864)**的笔记，与当前工程硬件不一致，仅供 FSMC 原理参考。
>
> **当前工程 `KuBei_Test` 实际硬件**（见 `Core/Src/lcd_ili9327.c`）：
> | 项目 | 当前值 |
> |---|---|
> | MCU | STM32F103VET6 (LQFP100) |
> | 屏 | ILI9327 彩色 TFT，240×400（0x2A/0x2B/0x2C 开窗方式，非 ILI9325 寄存器集） |
> | 总线 | FSMC Bank1 **NE1(PD7)**，16 位，RS(D/C) = **A16(PD11)** |
> | 地址 | 命令 `0x6000 0000`，数据 `0x6002 0000` |
> | 其它脚 | `LCD_RESET`=PC9，`LCD_BL`=PB0(TIM3_CH3 PWM)，`POW_SOFT_EN`=PE0，`POWER_KEY_TEST`=PE1 |

> 适用工程：`STM32_Calendar`（STM3210E-LK / STM32F103ZE，标准外设库 V3.1.0）
> 核心文件：`lcd.c`、`lcd.h`、`hw_config.c`、`main.c`

---

## 一、硬件与接口概况

| 项目 | 说明 |
|---|---|
| 屏 | ZYMG12864 单色点阵（128×64） |
| 控制器 | ST7565 / SED1565 系（命令 `0xE2` 复位、`0xA0` ADC、`0xC0/C8` COM 扫描、`0x81` 电子音量） |
| 总线 | **8080 并口，8 位**，用 `A0` 作 D/C（RS） |
| 访问方式 | 用 FSMC 把 8080 时序"伪装"成一块 SRAM，只写不读 |

### 地址映射

```c
#define LCD_Command  *((volatile unsigned char *)0x6c000000)  /* A0=0 命令 */
#define LCD_Data     *((volatile unsigned char *)0x6c000001)  /* A0=1 数据 */
```

- `0x6C000000` 落在 FSMC Bank1 第 4 子区（**NE4**，范围 0x6C00_0000~0x6FFF_FFFF）
- `bit0 = 0` → 命令口，`bit0 = 1` → 数据口，所以 **A0(PF0) 必须接 RS**
- 全工程只有写、没有读，读时序无关紧要

### 引脚分配（`LCD_CtrlLinesConfig`，均 AF_PP 50MHz）

| 引脚 | FSMC 功能 |
|---|---|
| PD14 / PD15 | D0 / D1 |
| PD0 / PD1 | D2 / D3 |
| PE7 ~ PE10 | D4 ~ D7 |
| PD4 / PD5 | NOE(RD) / NWE(WR) |
| PF0 | A0 (RS) |
| PG12 | NE4 (CS) |

打开的时钟：`RCC_AHBPeriph_FSMC` + `APB2: GPIOF/GPIOB/GPIOD/GPIOE/GPIOG/AFIO`。

### FSMC 参数（`LCD_FSMCConfig`）

| 参数 | 值 |
|---|---|
| Bank | FSMC_Bank1_NORSRAM4 (NE4) |
| DataAddressMux | Disable |
| MemoryType | SRAM |
| MemoryDataWidth | **8b**（必须） |
| Burst / Wrap / WaitSignal / ExtendedMode | Disable |
| Access mode | A |
| AddressSetupTime | 1 |
| AddressHoldTime | 1 |
| DataSetupTime | 20（≈290ns@72MHz） |
| BusTurnAroundDuration | 0 |
| CLKDivision / DataLatency | 0 / 1 |

---

## 二、STM32CubeMX（即 STM32Cube MX）配置

> 用户提到的 "Kube Max / fcms" 均按 STM32CubeMX 理解。

### 1. 时钟树
HSE → PLL×9 → SYSCLK = HCLK = PCLK2 = **72MHz**，PCLK1 = 36MHz；使能 FSMC 的 AHB 时钟。

### 2. FSMC 配置
- 左侧 `Connectivity → FSMC` → 点 **LCD** 标签 → 勾选 **LCD 1**，Chip Select 选 **NE4**
- 参数：

| 字段 | 设置 |
|---|---|
| LCD Register Select | **A0** |
| Data | **8 bits** |
| Write operation | Enabled |
| Extended mode | Disabled |
| Wait signal / Asynchronous wait | Disabled |
| Address setup time | 1 |
| Data setup time | 20 |
| Bus turn around | 0 |
| Access mode | A |

- LCD 标签下通常只暴露 Address/Data setup、Bus turn around、Access mode，Address hold 用默认即可。

### 3. GPIO 核对
CubeMX 自动分配：PF0=RS、PG12=NE4、PD4/5=NOE/NWE、PD14/15=D0/D1、PD0/1=D2/D3、PE7~10=D4~D7。把这些引脚 GPIO mode 设为 **Alternate Function Push-Pull**，Speed 设为 **High(50MHz)**。

### 4. 生成后衔接
- 保留 `lcd.h` 的两个指针宏 `0x6c000000` / `0x6c000001`（CubeMX 不会改它，只要 NE4+A0+8bit 对齐即可）。
- 二选一，避免重复初始化：
  - 用 CubeMX 生成的 `MX_FSMC_Init()`，则**删掉 `lcd.c` 里自写的 `LCD_CtrlLinesConfig()` 与 `LCD_FSMCConfig()`**；
  - 或保留原库不变，CubeMX 只生成 `main()` 框架且不调用 `MX_FSMC_Init()`。

### 5. 等价的 HAL 初始化（参考）

```c
SRAM_HandleTypeDef hsram;
FSMC_NORSRAM_TimingTypeDef Timing = {0};

Timing.AddressSetupTime      = 1;
Timing.AddressHoldTime       = 1;
Timing.DataSetupTime         = 20;
Timing.BusTurnAroundDuration = 0;
Timing.CLKDivision           = 0;
Timing.DataLatency           = 1;
Timing.AccessMode            = FSMC_ACCESS_MODE_A;

hsram.Instance  = FSMC_NORSRAM_DEVICE;
hsram.Extended  = FSMC_NORSRAM_EXTENDED_DEVICE;
hsram.Init.NSBank             = FSMC_NORSRAM_BANK4;            // -> 0x6C000000
hsram.Init.DataAddressMux     = FSMC_DATA_ADDRESS_MUX_DISABLE;
hsram.Init.MemoryType         = FSMC_MEMORY_TYPE_SRAM;
hsram.Init.MemoryDataWidth    = FSMC_NORSRAM_MEM_BUS_WIDTH_8;  // 必须 8 位
hsram.Init.BurstAccessMode    = FSMC_BURST_ACCESS_MODE_DISABLE;
hsram.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
hsram.Init.WrapMode           = FSMC_WRAP_MODE_DISABLE;
hsram.Init.WaitSignalActive   = FSMC_WAIT_TIMING_BEFORE_WS;
hsram.Init.WriteOperation     = FSMC_WRITE_OPERATION_ENABLE;
hsram.Init.WaitSignal         = FSMC_WAIT_SIGNAL_DISABLE;
hsram.Init.ExtendedMode       = FSMC_EXTENDED_MODE_DISABLE;
hsram.Init.WriteBurst         = FSMC_WRITE_BURST_DISABLE;
HAL_SRAM_Init(&hsram, &Timing, NULL);
```

---

## 三、LCD 屏完整初始化过程（从上电到点亮）

```
复位 → SystemInit → main()
  └─ Set_System()
       ├─ RCC_Config()              // 1. 系统时钟 72MHz + 开 FSMC/GPIO 时钟
       └─ STM3210E_LCD_Init()
            ├─ LCD_CtrlLinesConfig() // 2. FSMC 引脚复用推挽
            ├─ LCD_FSMCConfig()      // 3. FSMC Bank4(NE4) 8位/Mode A/只写
            └─ 发 LCD 控制器初始化命令序列
                 ├─ LCD_Clear()        // 5. 清屏(GRAM 全 0)
                 ├─ 设页/列起始地址     // 6. 光标归零
                 └─ Display_On (0xAF)  // 7. 开显示
  └─ main() 显示菜单 / 时间（RTC 每秒刷新）
```

### 第 1 步 时钟与 FSMC 时钟（`RCC_Config`）

```c
RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
RCC_PLLCmd(ENABLE);
while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOF | RCC_APB2Periph_GPIOB |
                       RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE |
                       RCC_APB2Periph_GPIOG | RCC_APB2Periph_AFIO, ENABLE);
```

### 第 2 步 FSMC 引脚（`LCD_CtrlLinesConfig`）

```c
// D0/D1=PD14/15, D2/D3=PD0/1, NOE=PD4, NWE=PD5
GPIO_Init(GPIOD, &(GPIO_InitTypeDef){
    .GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_14|GPIO_Pin_15,
    .GPIO_Speed = GPIO_Speed_50MHz, .GPIO_Mode = GPIO_Mode_AF_PP});
GPIO_Init(GPIOE, &(GPIO_InitTypeDef){   // D4~D7 = PE7~PE10
    .GPIO_Pin = GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_9|GPIO_Pin_10,
    .GPIO_Speed = GPIO_Speed_50MHz, .GPIO_Mode = GPIO_Mode_AF_PP});
GPIO_Init(GPIOF, &(GPIO_InitTypeDef){   // RS = A0 = PF0
    .GPIO_Pin = GPIO_Pin_0, .GPIO_Speed = GPIO_Speed_50MHz, .GPIO_Mode = GPIO_Mode_AF_PP});
GPIO_Init(GPIOG, &(GPIO_InitTypeDef){   // CS = NE4 = PG12
    .GPIO_Pin = GPIO_Pin_12, .GPIO_Speed = GPIO_Speed_50MHz, .GPIO_Mode = GPIO_Mode_AF_PP});
```

### 第 3 步 FSMC 控制器（`LCD_FSMCConfig`）

```c
FSMC_NORSRAMTimingInitTypeDef p = {
    .FSMC_AddressSetupTime = 1, .FSMC_AddressHoldTime = 1,
    .FSMC_DataSetupTime = 20, .FSMC_BusTurnAroundDuration = 0,
    .FSMC_CLKDivision = 0, .FSMC_DataLatency = 1, .FSMC_AccessMode = FSMC_AccessMode_A};

FSMC_NORSRAMInitTypeDef f = {
    .FSMC_Bank = FSMC_Bank1_NORSRAM4, .FSMC_DataAddressMux = FSMC_DataAddressMux_Disable,
    .FSMC_MemoryType = FSMC_MemoryType_SRAM, .FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_8b,
    .FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable, .FSMC_WaitSignal = FSMC_WaitSignal_Disable,
    .FSMC_WriteOperation = FSMC_WriteOperation_Enable, .FSMC_ExtendedMode = FSMC_ExtendedMode_Disable,
    .FSMC_WriteBurst = FSMC_WriteBurst_Disable,
    .FSMC_ReadWriteTimingStruct = &p, .FSMC_WriteTimingStruct = &p};
FSMC_NORSRAMInit(&f);
FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4, ENABLE);
```

### 第 4~7 步 LCD 控制器命令序列（`STM3210E_LCD_Init`）

```c
LCD_Command = Display_Off;          // 0xAE 先关显示，避免闪屏
LCD_Command = LCD_Reset;            // 0xE2 软件复位
reset_delay();
LCD_Command = Set_LCD_Bias_9;       // 0xA3 偏压比
LCD_Command = Set_ADC_Normal;       // 0xA0 列地址正向
LCD_Command = COM_Scan_Dir_Reverse; // 0xC8 COM 反向扫描（上下方向）
LCD_Command = Set_Start_Line_X|0x0; // 0x40 起始行 0

LCD_Command = 0x2c;  power_delay(); // 升压1 开
LCD_Command = 0x2e;  power_delay(); // 升压2 开
LCD_Command = 0x2f;  power_delay(); // 升压+稳压器全开（每步需 ~50ms）

LCD_Command = Set_Ref_Vol_Reg|0x05;  // 0x25 分压电阻比 = 5
LCD_Command = Set_Ref_Vol_Mode;      // 0x81 进入电子音量模式
LCD_Command = Set_Ref_Vol_Reg;       // 0x20 对比度值 = 0x20

LCD_Clear();                         // 清屏：GRAM 全部写 0

LCD_Command = Set_Page_Addr_X|0x0;   // 0xB0 页0
LCD_Command = Set_ColH_Addr_X|0x0;   // 0x10 列高4位=0
LCD_Command = Set_ColL_Addr_X|0x0;   // 0x00 列低4位=0

LCD_Command = Display_On;            // 0xAF 开显示 → 屏亮
```

### 命令速查表

| 命令 | 值 | 作用 |
|---|---|---|
| Display_Off | 0xAE | 关显示 |
| LCD_Reset | 0xE2 | 软件复位 |
| Set_LCD_Bias_9 | 0xA3 | 偏压比 |
| Set_ADC_Normal | 0xA0 | 列地址方向 |
| COM_Scan_Dir_Reverse | 0xC8 | COM 扫描方向（上下） |
| Set_Start_Line_X | 0x40+n | 显示起始行偏移 |
| 0x2C / 0x2E / 0x2F | — | 升压/稳压器逐级上电 |
| Set_Ref_Vol_Reg\|n | 0x20+n | 电阻比（对比度粗调） |
| Set_Ref_Vol_Mode | 0x81 | 电子音量模式，后接 1 字节 |
| Set_Page_Addr_X | 0xB0+n | 选页（0~7，8页×8=64行） |
| Set_ColH/ColL_Addr_X | 0x10+n / 0x00+n | 列地址高/低 4 位 |
| Display_On | 0xAF | 开显示 |

### 画点示例（写数据）

```c
LCD_Command = Set_Page_Addr_X | Xpage;
LCD_Command = Set_ColH_Addr_X | colh;
LCD_Command = Set_ColL_Addr_X | coll;
for (i = 0; i < 8; i++) { LCD_Data = *c++; delay(); }   // 上半字（8 字节）
LCD_Command = Set_Page_Addr_X | (Xpage + 1);             // 下半字
```

---

## 四、成败关键（3 个必须踩准的点）

1. **FSMC 时钟要先开**（`RCC_AHBPeriph_FSMC`），否则写 `0x6C000000` 直接 HardFault。
2. **8 位总线**。16 位模式下 CPU 地址 bit0 不出现在 A0 上，`0x6C000001` 的 RS 区分失效，屏收不到数据。
3. **NE4 + A0**。NE4 决定基址 `0x6C000000`；A0 决定命令/数据。若硬件 RS 接到 A1/A2，需同步把 `lcd.h` 宏改为 `0x6C000002` / `0x6C000004`。

---

## 五、已知小隐患

- **`LCD_Clear()` 的 `j` 未复位**：`j=128` 只赋值一次，`uint8_t j` 下溢后第 2~8 页实际各写 256 字节。因写的是 0x00，清屏结果仍正确但耗时翻倍；建议在 `for (i=0;i<8;i++)` 内部每次 `j=128`。
- **PD0/PD1 与 HSE 冲突**：PD0/PD1 同时是 OSC_IN/OSC_OUT 与 FSMC_D2/D3。144 脚封装官方评估板可用，但自画板若用无源晶振建议改用**有源时钟 + HSE Bypass**，避免晶振被数据线干扰。
