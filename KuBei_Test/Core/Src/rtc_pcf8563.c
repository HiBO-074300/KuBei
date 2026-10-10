/**
  ******************************************************************************
  * @file    rtc_pcf8563.c
  * @brief   PCF8563 RTC 实现（I2C1，地址 0x51）
  *
  *  寄存器（BCD 格式）：
  *    0x02 秒(bit7=VL 掉电标志)  0x03 分  0x04 时
  *    0x05 日  0x06 星期  0x07 月(bit7=世纪)  0x08 年
  ******************************************************************************
  */

#include "rtc_pcf8563.h"    /* 内部已包含 rtc_pcf8563_cfg.h（全部硬件配置） */
#include <string.h>

/* 基线时间：默认取"工程编译时刻"（每次编译自动更新） */
static RTC_Time_t s_baseline;
static uint8_t    s_baselineValid = 0u;

/* ---------------- BCD ---------------- */
static uint8_t BcdToBin(uint8_t b) { return (uint8_t)(((b >> 4) & 0x0Fu) * 10u + (b & 0x0Fu)); }
static uint8_t BinToBcd(uint8_t v) { return (uint8_t)(((v / 10u) << 4) | (v % 10u)); }

/* ---------------- 内部：原始读写 ---------------- */

/* 读 0x02~0x08 共 7 字节原始 BCD */
static uint8_t RTC_ReadRegs(uint8_t *buf)
{
    return (HAL_I2C_Mem_Read(&RTC_I2C_HANDLE, RTC_I2C_ADDR, RTC_REG_SEC,
                             I2C_MEMADD_SIZE_8BIT, buf, 7u, RTC_I2C_TIMEOUT) == HAL_OK) ? 0u : 1u;
}

static uint8_t RTC_WriteRegs(const uint8_t *buf)
{
    return (HAL_I2C_Mem_Write(&RTC_I2C_HANDLE, RTC_I2C_ADDR, RTC_REG_SEC,
                              I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, 7u, RTC_I2C_TIMEOUT) == HAL_OK) ? 0u : 1u;
}

static uint8_t RTC_IsBcdValid(const uint8_t *buf)
{
    /* 秒寄存器 bit7 = VL（掉电标志），1 = 时间不可信 */
    if ((buf[0] & 0x80u) != 0u) return 0u;
    if ((buf[1] & 0x7Fu) > 0x59u) return 0u;                    /* 分 */
    if ((buf[2] & 0x3Fu) > 0x23u) return 0u;                    /* 时 */
    if ((buf[3] & 0x3Fu) == 0u || (buf[3] & 0x3Fu) > 0x31u) return 0u;   /* 日 */
    if ((buf[5] & 0x1Fu) == 0u || (buf[5] & 0x1Fu) > 0x12u) return 0u;   /* 月 */
    if (buf[6] > 0x99u) return 0u;                              /* 年 */
    return 1u;
}

/* ---------------- 对外接口 ---------------- */

uint8_t RTC_ReadTime(RTC_Time_t *t)
{
    uint8_t buf[7];

    if (t == NULL) return 1u;
    if (RTC_ReadRegs(buf) != 0u) return 2u;
    if (RTC_IsBcdValid(buf) == 0u) return 3u;    /* VL=1 或数值非法 */

    t->sec     = BcdToBin(buf[0] & 0x7Fu);
    t->min     = BcdToBin(buf[1] & 0x7Fu);
    t->hour    = BcdToBin(buf[2] & 0x3Fu);
    t->day     = BcdToBin(buf[3] & 0x3Fu);
    t->weekday = BcdToBin(buf[4] & 0x07u);
    t->month   = BcdToBin(buf[5] & 0x1Fu);
    t->year    = BcdToBin(buf[6]);

    return 0u;
}

uint8_t RTC_SetTime(const RTC_Time_t *t)
{
    uint8_t buf[7];

    if (t == NULL) return 1u;
    if (t->month < 1u || t->month > 12u) return 2u;
    if (t->day < 1u || t->day > 31u) return 2u;
    if (t->hour > 23u || t->min > 59u || t->sec > 59u) return 2u;

    /* 秒寄存器 bit7 写 0 = 清除掉电标志 VL */
    buf[0] = BinToBcd(t->sec) & 0x7Fu;
    buf[1] = BinToBcd(t->min);
    buf[2] = BinToBcd(t->hour);
    buf[3] = BinToBcd(t->day);
    buf[4] = BinToBcd(t->weekday & 0x07u);
    buf[5] = BinToBcd(t->month);
    buf[6] = BinToBcd(t->year);

    return RTC_WriteRegs(buf);
}

uint8_t RTC_IsTimeValid(const RTC_Time_t *t)
{
    if (t == NULL) return 0u;
    if (t->month < 1u || t->month > 12u) return 0u;
    if (t->day < 1u || t->day > 31u) return 0u;
    if (t->hour > 23u || t->min > 59u || t->sec > 59u) return 0u;
    return 1u;
}

int8_t RTC_Compare(const RTC_Time_t *a, const RTC_Time_t *b)
{
    if (a == NULL || b == NULL) return 0;

    if (a->year  != b->year)  return (a->year  < b->year)  ? -1 : 1;
    if (a->month != b->month) return (a->month < b->month) ? -1 : 1;
    if (a->day   != b->day)   return (a->day   < b->day)   ? -1 : 1;
    if (a->hour  != b->hour)  return (a->hour  < b->hour)  ? -1 : 1;
    if (a->min   != b->min)   return (a->min   < b->min)   ? -1 : 1;
    if (a->sec   != b->sec)   return (a->sec   < b->sec)   ? -1 : 1;
    return 0;
}

void RTC_SetBaseline(const RTC_Time_t *t)
{
    if (t != NULL) { s_baseline = *t; s_baselineValid = 1u; }
}

void RTC_GetBaseline(RTC_Time_t *t)
{
    if (t != NULL && s_baselineValid != 0u) *t = s_baseline;
}

/* 从 __DATE__ "Mmm dd yyyy" / __TIME__ "hh:mm:ss" 解析出基线时间 */
static void RTC_LoadBuildTime(void)
{
    static const char months[12][4] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    const char *d = __DATE__;
    const char *t = __TIME__;
    uint8_t i, mon = 1u, day = 1u, yr = 0u, hh = 0u, mi = 0u, ss = 0u;

    for (i = 0u; i < 12u; i++)
    {
        if (d[0] == months[i][0] && d[1] == months[i][1] && d[2] == months[i][2])
        {
            mon = (uint8_t)(i + 1u);
            break;
        }
    }
    day = (uint8_t)(((d[4] - '0') * 10u) + (d[5] - '0'));
    yr  = (uint8_t)(((d[9] - '0') * 10u) + (d[10] - '0'));

    hh = (uint8_t)(((t[0] - '0') * 10u) + (t[1] - '0'));
    mi = (uint8_t)(((t[3] - '0') * 10u) + (t[4] - '0'));
    ss = (uint8_t)(((t[6] - '0') * 10u) + (t[7] - '0'));

    s_baseline.year  = yr;
    s_baseline.month = mon;
    s_baseline.day   = day;
    s_baseline.hour  = hh;
    s_baseline.min   = mi;
    s_baseline.sec   = ss;
    s_baseline.weekday = 0u;
    s_baselineValid  = 1u;
}

/**
  * @brief  上电时间同步（按需求实现的核心逻辑）：
  *         读 RTC -> 若时间无效(掉电标志/非法/小于基线) -> 写入基线时间
  */
uint8_t RTC_SyncBaseline(void)
{
    RTC_Time_t t;

    if (s_baselineValid == 0u) RTC_LoadBuildTime();

    if (RTC_ReadTime(&t) != 0u)                 /* 读失败 / VL=1 / 数值非法 */
    {
        return RTC_SetTime(&s_baseline);
    }
    if (RTC_Compare(&t, &s_baseline) < 0)       /* 读出的时间 < 基线：掉电过 */
    {
        return RTC_SetTime(&s_baseline);
    }
    return 0u;                                   /* 不小于基线：不设置 */
}

uint8_t RTC_Init(void)
{
    uint8_t  buf[7];
    RTC_Time_t probe;

    /* 探测芯片是否应答（读 1 字节） */
    if (HAL_I2C_Mem_Read(&RTC_I2C_HANDLE, RTC_I2C_ADDR, RTC_REG_SEC,
                         I2C_MEMADD_SIZE_8BIT, buf, 1u, RTC_I2C_TIMEOUT) != HAL_OK)
    {
        return 1u;                               /* 芯片不应答：查接线/上拉/地址 */
    }

    RTC_LoadBuildTime();

    if (RTC_SyncBaseline() != 0u) return 2u;     /* 同步失败 */

    /* 再读一次确认可用 */
    if (RTC_ReadTime(&probe) != 0u) return 3u;

    return 0u;
}
