/**
  ******************************************************************************
  * @file    tm1668.h
  * @brief   TM1668 按键扫描驱动 —— 对外 API
  *
  *  ★ 硬件相关内容（引脚、时序、采样沿）全部在 tm1668_cfg.h 中，
  *    换硬件只改那个文件，本文件只有接口声明。
  *
  *  键盘矩阵：K1/K2 x SEG1~SEG10 = 20 个键
  *      键索引 idx = (KS-1)*2 + (K-1)      0~19
  *      原始数据 5 字节：第 n 字节只容纳 2 条 KS 线（BYTE1=KS1/2 … BYTE5=KS9/10）
  *        bit0/1 = KS(2n+1) 的 K1/K2，bit3/4 = KS(2n+2) 的 K1/K2，
  *        bit2/5/6/7 为无效位 -> 压缩成低 4 位后就是一行 4 个键
  *
  *  键值：
  *      键位表 s_keyMap[5][4]（见 tm1668.c）把每个键映射成一个字符，
  *      行 = 键扫第 n 字节，列 = 压缩后的低 4 位 bit0~3。
  *      TM1668_GetKey()         返回键号（索引+1，即 1~20）；
  *      TM1668_GetPressedKeys() 返回当前按下的字符，按按下先后顺序排列。
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
#define TM1668_KEY_LIST_MAX     8u    /* 按下列表最多同时记录几个键 */

/* ---------------- 接口 ---------------- */

/* 初始化：把三个脚设到协议要求的空闲态（STB=H、CLK=H、DIO=释放），并打开显示（键扫需要） */
uint8_t  TM1668_Init(void);

/* 读一次键扫原始数据：返回 20 位按下掩码，bit idx = 1 表示 键idx 按下 */
uint32_t TM1668_ReadKeyMask(void);

/* 主循环周期调用（建议 20~50ms 一次）。内部含去抖，并顺带刷新按下列表。
 * 返回值 = 本次"新按下"的键号（1~20，0 = 无按键事件） */
uint8_t  TM1668_GetKey(void);

/* 同上，但返回键位表里的字符（'\0' = 无按键事件）。
 * 与 TM1668_GetKey() 共用一次扫描，主循环里只能选调其中一个 */
char     TM1668_GetKeyChar(void);

/* 取当前按下的键（按按下先后顺序）：out 依次写入键位表里的字符并补 '\0'
 * maxLen = out 的容量（含 '\0'），建议用 TM1668_KEY_LIST_MAX + 1
 * 返回值 = 按下的键个数；松开的键会从列表里删除，后面的键自动前移 */
uint8_t  TM1668_GetPressedKeys(char *out, uint8_t maxLen);

/* 显示控制（本工程 SEG1~10 兼作键扫描线；如需点亮 LED/数码管可用）
 * 命令位布局以 TM1668 手册为准：0x80 | 亮度(bit0~2) | 显示开关(bit3) */
void     TM1668_SetDisplay(uint8_t on, uint8_t brightness);

#ifdef __cplusplus
}
#endif

#endif /* TM1668_H */
