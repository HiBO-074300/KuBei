/**
  ******************************************************************************
  * @file    power_ctrl.h
  * @brief   按键开机 + 软件维持供电(自锁) + 长按关机
  *
  *  开机键 POWER_KEY_TEST = PE1：按下 = 高电平，此时系统得电；
  *  松开后需由 POW_SOFT_EN = PE0 输出高电平维持供电；PE0 拉低即断电。
  ******************************************************************************
  */

#ifndef POWER_CTRL_H
#define POWER_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 按键是否启用芯片内部下拉。
 * CubeMX 里 PE1 是无上下拉，引脚悬空时读数不稳会导致“长按关机”失效，
 * 这里在代码里补上内部下拉（若你的板子外部已有下拉，可设为 0）。 */
#ifndef POWER_KEY_USE_PULLDOWN
#define POWER_KEY_USE_PULLDOWN  1
#endif

/* main() 的最开头调用：在任何初始化之前先把 PE0 拉高，防止掉电 */
void     POWER_SelfHoldEarly(void);

/* MX_GPIO_Init() 之后调用：用 HAL 再确认一次 PE0 = 1，并配置按键输入 */
void     POWER_SelfHold(void);

/* 配置按键引脚（内部下拉 + 去抖参数初始化），在 MX_GPIO_Init() 之后调用 */
void     POWER_Init(void);

/* 按键原始电平：1 = 高电平(按下)，0 = 低电平(松开) */
uint8_t  POWER_KeyRaw(void);

/* 主循环每轮都要调用：跑按键去抖、长按计时 */
void     POWER_Poll(uint32_t nowMs);

/* 去抖后的稳定状态：1 = 按下 */
uint8_t  POWER_KeyIsPressed(void);

/* 本次已持续按住多少毫秒，未按下返回 0 */
uint32_t POWER_PressDuration(void);

/* 长按判断：按住超过 holdMs 且已解锁(见过一次松开)时返回 1 */
uint8_t  POWER_LongPress(uint32_t holdMs);

/* 断电：PE0 拉低并一直保持（函数不返回） */
void     POWER_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_CTRL_H */
