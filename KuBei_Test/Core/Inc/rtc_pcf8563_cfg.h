/**
  ******************************************************************************
  * @file    rtc_pcf8563_cfg.h
  * @brief   PCF8563 RTC 硬件配置（唯一需要改动的文件）
  *
  *  ★ 换 I2C / 换地址只改这个文件，rtc_pcf8563.c 里不出现具体句柄 ★
  *
  * ============================ 本板硬件连接 ============================
  *
  *  芯片   : PCF8563 实时时钟（I2C 接口）
  *  总线   : I2C1
  *
  *  | 信号 | 引脚 | CubeMX 配置                    | 说明                        |
  *  |------|------|--------------------------------|-----------------------------|
  *  | SCL  | PB6  | I2C1_SCL, AF 开漏, 速度 High   | 必须外接上拉电阻 4.7K~10K   |
  *  | SDA  | PB7  | I2C1_SDA, AF 开漏, 速度 High   | 必须外接上拉电阻 4.7K~10K   |
  *
  *  地址   : 7 位地址 0x51（芯片 SA0 引脚接地时的固定地址，不可改）
  *  电平   : PCF8563 支持 1.8~5.5V；若芯片 5V 供电而 MCU 3.3V，
  *           I2C 上拉务必只拉到 3.3V（开漏总线天然支持分压供电方案），
  *           或确认 PCF8563 侧 VIH 满足 0.7*VDD 再决定上拉电压。
  *  速度   : 100kHz / 400kHz 均可（CubeMX I2C1 的 Clock speed）
  *  电池   : VDD 掉电后由纽扣电池维持走时；掉电标志 VL（秒寄存器 bit7）会置 1，
  *           驱动据此判断"时间无效"并按基线策略重新写入。
  ******************************************************************************
  */

#ifndef RTC_PCF8563_CFG_H
#define RTC_PCF8563_CFG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "i2c.h"      /* hi2c1 */

/* ---------------- I2C 配置（改这里即可换 I2C 外设） ---------------- */
#define RTC_I2C_HANDLE      hi2c1                     /* CubeMX: I2C1 */

/* 7 位地址 0x51 -> HAL 用 8 位格式，左移 1 位 */
#define RTC_I2C_ADDR        (0x51u << 1)

#define RTC_I2C_TIMEOUT     100u                      /* 单次 I2C 传输超时 ms */

/* ---------------- 寄存器地址（芯片固定，一般不用改） ---------------- */
#define RTC_REG_SEC         0x02u    /* 秒  VL=bit7 */
#define RTC_REG_MIN         0x03u
#define RTC_REG_HOUR        0x04u
#define RTC_REG_DAY         0x05u
#define RTC_REG_WEEKDAY     0x06u
#define RTC_REG_MONTH       0x07u
#define RTC_REG_YEAR        0x08u

#ifdef __cplusplus
}
#endif

#endif /* RTC_PCF8563_CFG_H */
