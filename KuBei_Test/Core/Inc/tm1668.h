/**
  ******************************************************************************
  * @file    tm1668.h
  * @brief   TM1668 按键扫描驱动 —— 对外 API
  *
  *  ★ 硬件相关内容（引脚、时序、采样沿）全部在 tm1668_cfg.h 中，
  *    换硬件只改那个文件，本文件只有接口声明。
  *
  *  键盘矩阵：K1/K2 x SEG1~SEG10 = 20 个键
  *      键索引 idx = (SEG-1)*2 + (K-1)      0~19
  *      原始数据 5 字节：第 n 字节的 bit[2m]/bit[2m+1]
  *        对应 K1/K2 与 SEG(4n+m+1)
  *
  *  键值映射：
  *      每个物理键映射成一个"键值"，上层只判断键值即可；
  *      默认 键值 = 索引+1（1~20），可用 TM1668_SetKeyMap() 换成任意值。
  ******************************************************************************
  */

#ifndef TM1668_H
#define TM1668_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tm1668_cfg.h"   /* ← 所有硬件配置（引脚/时序/采样沿） */

/* ---------------- 参数 ---------------- */
#define TM1668_KEY_NUM          20u   /* K1/K2 x SEG1~SEG10 */
#define TM1668_KEY_DATA_BYTES   5u    /* 键值数据 5 字节 */

/* ---------------- 接口 ---------------- */

/* 初始化：把三个脚设到协议要求的空闲态（STB=H、CLK=H、DIO=释放） */
uint8_t  TM1668_Init(void);

/* 读一次键扫原始数据：返回 20 位按下掩码，bit idx = 1 表示 键idx 按下 */
uint16_t TM1668_ReadKeyMask(void);

/* 主循环周期调用（建议 20~50ms 一次）。内部含去抖。
 * 返回值 = 本次"新按下"的键的键值（0 = 无按键事件） */
uint8_t  TM1668_GetKey(void);

/* 自定义键值映射表：20 字节，tm1668.h 里索引对应的键值；NULL = 默认（索引+1） */
void     TM1668_SetKeyMap(const uint8_t *map20);
uint8_t  TM1668_GetLastKey(void);    /* 最近一次上报的键值（调试/显示用） */

/* 显示控制（本工程 SEG1~10 兼作键扫描线；如需点亮 LED/数码管可用）
 * 命令位布局以 TM1668 手册为准 */
void     TM1668_SetDisplay(uint8_t on, uint8_t brightness);

#ifdef __cplusplus
}
#endif

#endif /* TM1668_H */
