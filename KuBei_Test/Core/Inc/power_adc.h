/**
  ******************************************************************************
  * @file    power_adc.h
  * @brief   电源电压采集：PC0(充电/电源) + PC1(电池)，均为 33K/33K 分压
  *
  *  硬件：
  *      PC0 -> ADC1_IN10 -> 分压点（"ADC_POWER"）   实际电压 = ADC 电压 x 2
  *      PC1 -> ADC1_IN11 -> 分压点（"ADC_BAT"）     实际电压 = ADC 电压 x 2
  *      两个 33K 串联分压：Vadc = Vx * 33 / (33 + 33) = Vx / 2
  *
  *  注意：
  *      1) 分压后源阻抗 = 33K || 33K = 16.5K，采样时间必须放宽到 239.5 周期，
  *         否则采样电容充不满，读数会明显偏低（CubeMX 默认的 1.5 周期太小）。
  *      2) 默认按 VDDA = 3300mV 折算；若你的板子不是稳压 3.3V（例如直接由电池
  *         供电），把 PWR_ADC_USE_VREFINT 打开，用内部 1.2V 参考实时估算真实 VDDA。
  ******************************************************************************
  */

#ifndef POWER_ADC_H
#define POWER_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "adc.h"

/* ---------------- 用户可改参数 ---------------- */
#ifndef PWR_ADC_VREF_MV
#define PWR_ADC_VREF_MV       3300u   /* 参考电压 VDDA（mV） */
#endif

#ifndef PWR_ADC_DIV_DEN
#define PWR_ADC_DIV_DEN       2u      /* 分压分母：两个 33K -> 1/2 */
#endif

#ifndef PWR_ADC_DIV_NUM
#define PWR_ADC_DIV_NUM       1u      /* 分压分子 */
#endif

#ifndef PWR_ADC_AVG_SAMPLES
#define PWR_ADC_AVG_SAMPLES   16u     /* 每次读取的采样次数（去掉最大最小后取平均） */
#endif

#ifndef PWR_ADC_USE_VREFINT
#define PWR_ADC_USE_VREFINT   0       /* 1 = 用内部 1.2V 参考实时估算 VDDA 参与折算 */
#endif

/**
  * ADC 配置模式，必须与 CubeMX 里的设置一致：
  *   1 = 扫描双通道（ScanConvMode = Enable, NbrOfConversion = 2,
  *        Rank1 = 通道10(PC0), Rank2 = 通道11(PC1)）—— 当前工程用的就是这个；
  *        一次 Start 顺序读出两个通道，ContinuousConvMode 开/关都兼容
  *        （建议关掉连续模式：省电，也不会出现"读到另一个通道"的混淆）
  *   0 = 单通道轮询（ScanConvMode = Disable, NbrOfConversion = 1）
  *        每次读之前由软件切换通道
  */
#ifndef PWR_ADC_SEQ_SCAN
#define PWR_ADC_SEQ_SCAN      0
#endif

/* 通道索引 */
#define PWR_ADC_CH_POWER      0u      /* PC0：充电/电源电压 */
#define PWR_ADC_CH_BAT        1u      /* PC1：电池电压 */

/* ---------------- 接口 ---------------- */
uint8_t  PowerAdc_Init(void);                    /* 0 = 成功（含 ADC 自校准） */
uint8_t  PowerAdc_IsReady(void);

/* ADC 自校准：可随时调用（内部会先 Stop 再校准）。上电/复位后各调一次最稳：
 * 复位后立刻校准 + 电源/背光稳定后再校准一次。0 = 成功 */
uint8_t  PowerAdc_Calibrate(void);

/* 最近一次采样/校准的错误码（0 = 无错误）：
 *   1/2/3 = 第 1 次 Start / rank1 转换 / rank2 转换 失败
 *   0x10 以上 = 对应错误发生在多组平均内部
 *   0x20   = 校准失败  */
uint8_t  PowerAdc_LastError(void);

uint16_t PowerAdc_ReadRaw(uint8_t ch);           /* 0~4095，已做多次采样滤波 */
uint32_t PowerAdc_RawToMv(uint16_t raw);         /* 原始值 -> 测量点实际电压(mV) */
uint32_t PowerAdc_ReadMv(uint8_t ch);            /* 直接读某个通道的实际电压(mV) */

uint32_t PowerAdc_PowerMv(void);                 /* PC0 充电/电源电压(mV) */
uint32_t PowerAdc_BatMv(void);                   /* PC1 电池电压(mV) */

/* 用内部 1.2V 参考估算真实 VDDA（诊断用；VREFINT 本身有 ±3% 左右误差） */
uint32_t PowerAdc_ReadVddaMv(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_ADC_H */
