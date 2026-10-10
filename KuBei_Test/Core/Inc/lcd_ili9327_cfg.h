/**
  ******************************************************************************
  * @file    lcd_ili9327_cfg.h
  * @brief   LCD 硬件配置（唯一需要改动的文件）
  *
  *  ★ 换屏 / 换引脚 / 改总线速度，只改这一个文件 ★
  *    lcd_ili9327.c / disp_ili9327.c / gui.c 里不出现任何具体引脚和地址。
  *
  * ============================ 本板硬件连接 ============================
  *
  *  控制器 : ILI9327（注意：寄存器集与 ILI9325/9328 完全不同，
  *           采用 0x2A/0x2B 开窗 + 0x2C 写 GRAM，同 ILI9341 风格）
  *  总线   : FSMC Bank1 / NE1，8080 并口，16 位数据
  *
  *  --- FSMC 引脚（STM32F1 的 FSMC 复用脚是芯片固定映射，不能任意更换）---
  *      NE1  = PD7   片选（低有效），决定基址 0x60000000
  *      NWE  = PD5   写使能
  *      NOE  = PD4   读使能
  *      RS   = A16 = PD11  命令/数据选择（0=命令，1=数据）
  *      D0   = PD14     D1  = PD15    D2  = PD0     D3  = PD1
  *      D4   = PE7      D5  = PE8     D6  = PE9     D7  = PE10
  *      D8   = PE11     D9  = PE12    D10 = PE13    D11 = PE14
  *      D12  = PE15     D13 = PD8     D14 = PD9     D15 = PD10
  *      （以上数据/控制脚全部配成 AF 推挽输出，速度 High）
  *
  *  --- 普通 GPIO ---
  *      LCD_RESET = PC9   复位，推挽输出，初始 High（低电平复位 ≥10us）
  *      LCD_BL    = PC8   背光，TIM3_CH3 PWM（需打开 TIM3 完全重映射：
  *                        __HAL_AFIO_REMAP_TIM3_ENABLE()，CubeMX 生成在 tim.c）
  *
  * ============================ 地址映射推导 ============================
  *
  *  NE1 子区基址 = 0x6000 0000（FSMC Bank1 第 1 子区，0x6000_0000 ~ 0x63FF_FFFF）
  *  CPU 地址 HADDR[25:1] 依次接 FSMC 的 A[24:0]；16 位数据总线时：
  *      FSMC 的 A16  对应 CPU 地址位 HADDR[17]，即字节偏移 1 << 17 = 0x2_0000
  *  所以：
  *      写 0x6000_0000 -> RS(A16) = 0 -> 屏收到"命令"
  *      写 0x6002_0000 -> RS(A16) = 1 -> 屏收到"数据"
  *  （这就是 LCD_REG / LCD_RAM 两个宏的来历；若 RS 接在 A17 上，
  *    把下面 LCD_RAM_ADDR 的偏移改成 1<<18 = 0x4_0000 即可）
  *
  * ============================ FSMC 时序参数 ============================
  *
  *  单位：HCLK 时钟周期（72MHz 时 1 周期 ≈ 13.9ns）
  *  一次写访问 ≈ (AddressSetup + DataSetup + 1) 个周期
  *  CubeMX 字段位置：Connectivity -> FSMC -> NORSRAM1 -> Parameter Settings
  *
  *  | CubeMX 字段                  | 建议值 | 说明                          |
  *  |------------------------------|--------|-------------------------------|
  *  | Memory type                  | SRAM   | 8080 接口按 SRAM 方式配       |
  *  | Data width                   | 16 bits| 必须 16 位                    |
  *  | Write operation              | Enable |                               |
  *  | Extended mode                | Disable| 读写共用一套时序              |
  *  | Address setup time           | 2      | RS/CS 建立时间                |
  *  | Data setup time              | 10     | WR 低电平宽度，约 139ns，      |
  *  |                              |        | 读 ID 不稳可加大到 15~20      |
  *  | Bus turn around              | 0      | 读写切换周转                  |
  *  | Access mode                  | A      |                               |
  *
  *  注意：lcd_ili9327.c 的 LCD_FSMC_Timing() 会在 LCD_Init() 时用下面的宏
  *  重新调用 HAL_SRAM_Init() 覆盖 CubeMX 的值（双保险）。
  ******************************************************************************
  */

#ifndef LCD_ILI9327_CFG_H
#define LCD_ILI9327_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "fsmc.h"     /* hsram1 */
#include "tim.h"      /* htim3  */

/* ---------------- 1. 复位脚 ---------------- */
/* CubeMX: PC9 -> GPIO_Output，推挽，No pull，Low speed，Output level = High */
#define LCD_RST_PORT        LCD_RESET_GPIO_Port
#define LCD_RST_PIN         LCD_RESET_Pin

/* ---------------- 2. 背光 PWM ---------------- */
/* CubeMX: PC8 -> TIM3_CH3（需勾选 TIM3 完全重映射），CH Polarity High，
 *         PWM Mode 1，Pulse 任意（驱动会在 LCD_BL_Init 里设占空比） */
#define LCD_BL_TIM          (&htim3)
#define LCD_BL_CH           TIM_CHANNEL_3
#define LCD_BL_FREQ_DEF     50u     /* 上电默认亮度 % */

/* ---------------- 3. FSMC 句柄与时序 ---------------- */
/* CubeMX: Connectivity -> FSMC -> NORSRAM1（NE1，SRAM，16 bits） */
#define LCD_HSRAM           hsram1

#define LCD_FSMC_ADDSET     2u      /* AddressSetupTime      */
#define LCD_FSMC_ADDHOLD    0u      /* AddressHoldTime       */
#define LCD_FSMC_DATSET     10u     /* DataSetupTime         */
#define LCD_FSMC_BTA        0u      /* BusTurnAroundDuration */

/* ---------------- 4. FSMC 地址映射 ---------------- */
#define LCD_FSMC_BASE       ((uint32_t)0x60000000UL)
#define LCD_REG_ADDR        (LCD_FSMC_BASE)                 /* A16=0 : 命令 */
#define LCD_RAM_ADDR        (LCD_FSMC_BASE + 0x00020000UL)  /* A16=1 : 数据 */

#define LCD_REG             (*(volatile uint16_t *)LCD_REG_ADDR)
#define LCD_RAM             (*(volatile uint16_t *)LCD_RAM_ADDR)

/* ---------------- 5. 分辨率与方向 ---------------- */
/* 面板原生 240 x 320，本机横放 => 逻辑画面 320 宽 x 240 高 */
#ifndef LCD_W
#define LCD_W               320u
#endif
#ifndef LCD_H
#define LCD_H               240u
#endif
/* 控制器 GRAM 原生 240x432；未写入的行会显示成黑白条纹，
 * LCD_ClearPanel() 会把 432x432 整片刷掉 */
#ifndef LCD_GRAM_H
#define LCD_GRAM_H          432u
#endif

/* 0x36(MADCTL)：MY=0x80 MX=0x40 MV=0x20 BGR=0x08
 * 横屏四种朝向：0x28 / 0x68(左右镜像) / 0xA8(上下镜像) / 0xE8(旋转180°)
 * 红蓝颠倒：去掉 BGR 位（0x28 -> 0x20） */
#ifndef LCD_MADCTL
#define LCD_MADCTL          0xE8
#endif

/* 窗口映射交换：0 = 0x2A<-x, 0x2B<-y；画面横竖颠倒时取反 */
#ifndef LCD_SWAP_XY
#define LCD_SWAP_XY         0
#endif

#ifdef __cplusplus
}
#endif

#endif /* LCD_ILI9327_CFG_H */
