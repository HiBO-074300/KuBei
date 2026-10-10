/**
  ******************************************************************************
  * @file    rtc_pcf8563.h
  * @brief   PCF8563 RTC 驱动（I2C1，7 位地址 0x51）
  *
  *  时间有效性策略（按需求实现）：
  *    1) 上电第一次：把时间设置为"基线时间"（默认 = 工程编译时刻）；
  *    2) 以后每次上电：读 RTC 时间，与基线比较——
  *         - 读出的时间 < 基线（说明 RTC 掉电/复位过）  -> 重新写入基线时间
  *         - 读出的时间 >= 基线                          -> 不设置，直接使用
  *    3) 另外芯片自身的掉电标志（秒寄存器 bit7 VL=1）或数值非法也按无效处理。
  ******************************************************************************
  */

#ifndef RTC_PCF8563_H
#define RTC_PCF8563_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rtc_pcf8563_cfg.h"   /* ← 所有硬件配置（I2C 句柄/地址/寄存器） */

typedef struct
{
    uint8_t year;     /* 00~99，实际年份 = 2000 + year */
    uint8_t month;    /* 1~12 */
    uint8_t day;      /* 1~31 */
    uint8_t weekday;  /* 0~6（PCF8563 的星期寄存器，含义由你定义） */
    uint8_t hour;     /* 0~23 */
    uint8_t min;      /* 0~59 */
    uint8_t sec;      /* 0~59 */
} RTC_Time_t;

/* 初始化：探测芯片 + 按上面的策略同步时间。0 = 成功（时间可用） */
uint8_t RTC_Init(void);

/* 读时间。返回 0 = 成功；t->sec 不会带 VL 标志（无效时接口返回错误码） */
uint8_t RTC_ReadTime(RTC_Time_t *t);

/* 写时间（同时清除掉电标志 VL） */
uint8_t RTC_SetTime(const RTC_Time_t *t);

/* 时间有效性检查（数值范围合法，且 >= 基线才算"可信"由调用方决定） */
uint8_t RTC_IsTimeValid(const RTC_Time_t *t);

/* 时间比较：a<b 返回 -1，相等 0，a>b 返回 1 */
int8_t  RTC_Compare(const RTC_Time_t *a, const RTC_Time_t *b);

/* 基线时间（默认 = 编译时刻），可在 RTC_Init 前自定义 */
void    RTC_SetBaseline(const RTC_Time_t *t);
void    RTC_GetBaseline(RTC_Time_t *t);

/* 执行"掉电检测 -> 必要时重设"逻辑（RTC_Init 内部已调用） */
uint8_t RTC_SyncBaseline(void);

#ifdef __cplusplus
}
#endif

#endif /* RTC_PCF8563_H */
