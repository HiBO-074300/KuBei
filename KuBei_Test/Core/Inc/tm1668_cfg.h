/**
  ******************************************************************************
  * @file    tm1668_cfg.h
  * @brief   TM1668 按键芯片硬件配置（唯一需要改动的文件）
  *
  *  ★ 换引脚只改这个文件，tm1668.c 里不出现任何具体 GPIO ★
  *
  * ============================ 本板硬件连接 ============================
  *
  *  芯片   : TM1668（LED 驱动 + 键盘扫描，键盘矩阵 K1/K2 x SEG1~SEG10 = 20 键）
  *  接口   : 三线串行（类似 SPI 但非标准 SPI，纯 GPIO 软件时序）
  *           不要挂到 I2C/SPI 硬件外设上
  *
  *  | 信号 | 引脚 | CubeMX 配置                       | 说明                       |
  *  |------|------|-----------------------------------|----------------------------|
  *  | STB  | PE2  | GPIO_Output, 推挽, No pull, Low   | 片选/帧使能，空闲必须为高   |
  *  | CLK  | PE3  | GPIO_Output, 推挽, No pull, Low   | 时钟，空闲为高              |
  *  | DIO  | PE4  | GPIO_Output **Open-Drain + Pull-up** | 双向数据线（关键配置！）  |
  *
  *  ★ DIO 必须配成"开漏输出 + 上拉"的原因：
  *     TM1668 的 DIO 本身是开漏结构，线上必须上拉（内部上拉或外部 4.7K~10K）；
  *     开漏模式下"写 1 = 释放总线（靠上拉变高）"，此时读输入寄存器(IDR)得到的
  *     就是引脚真实电平 —— 所以读键值不用把引脚切成输入模式，也绝不会和芯片"对拉"。
  *
  *  ★ 上电初始电平：CubeMX 里三个脚默认都是 Low；TM1668_Init() 会把它们
  *     置到协议要求的空闲态（STB=H、CLK=H、DIO=释放），上电不会误发命令。
  *
  *  时序：位间隔约 1~2us（TM1668 最小时钟脉宽几百 ns，余量充足）。
  ******************************************************************************
  */

#ifndef TM1668_CFG_H
#define TM1668_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ---------------- 引脚定义（改这里即可换脚） ---------------- */
#define TM1668_STB_PORT     KEY_STB_GPIO_Port     /* PE2 */
#define TM1668_STB_PIN      KEY_STB_Pin
#define TM1668_CLK_PORT     KEY_CLK_GPIO_Port     /* PE3 */
#define TM1668_CLK_PIN      KEY_CLK_Pin
#define TM1668_DIO_PORT     DIO_GPIO_Port         /* PE4 */
#define TM1668_DIO_PIN      DIO_Pin

/* ---------------- 操作宏 ---------------- */
#define TM_STB_H()     HAL_GPIO_WritePin(TM1668_STB_PORT, TM1668_STB_PIN, GPIO_PIN_SET)
#define TM_STB_L()     HAL_GPIO_WritePin(TM1668_STB_PORT, TM1668_STB_PIN, GPIO_PIN_RESET)
#define TM_CLK_H()     HAL_GPIO_WritePin(TM1668_CLK_PORT, TM1668_CLK_PIN, GPIO_PIN_SET)
#define TM_CLK_L()     HAL_GPIO_WritePin(TM1668_CLK_PORT, TM1668_CLK_PIN, GPIO_PIN_RESET)
#define TM_DIO_H()     HAL_GPIO_WritePin(TM1668_DIO_PORT, TM1668_DIO_PIN, GPIO_PIN_SET)
#define TM_DIO_L()     HAL_GPIO_WritePin(TM1668_DIO_PORT, TM1668_DIO_PIN, GPIO_PIN_RESET)
#define TM_DIO_READ()  HAL_GPIO_ReadPin(TM1668_DIO_PORT, TM1668_DIO_PIN)

/* ---------------- 时序参数 ---------------- */
/* 位间隔的空转循环次数（volatile 循环不会被优化掉），
 * 72MHz 下约 24 次循环 ≈ 1.3us；若总线过长/干扰大可加大 */
#ifndef TM1668_BIT_DELAY_LOOPS
#define TM1668_BIT_DELAY_LOOPS    24u
#endif
#ifndef TM1668_FRAME_DELAY_LOOPS
#define TM1668_FRAME_DELAY_LOOPS  60u
#endif

/* 初始化时发送的显示模式命令（参考例程用 0x03），
 * 具体取值以 TM1668 手册的"显示模式设置"为准 */
#ifndef TM1668_MODE_CMD
#define TM1668_MODE_CMD            0x03u
#endif

#ifdef __cplusplus
}
#endif

#endif /* TM1668_CFG_H */
