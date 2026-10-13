/**
  ******************************************************************************
  * @file    serial_term.h
  * @brief   串口终端：USART3(9600, PC10/PC11) 中断接收，屏上滚动显示
  *
  *  - 接收走中断 + 环形缓冲（主循环刷屏可能超过 9600 的字节间隔，轮询会丢字节）
  *  - 显示模式：文本(ASCII + GB2312 汉字) / HEX（每字节两位十六进制），可随时切换
  *  - 字体：字库 8x16 ASCII + 15x16 汉字（等高混排），每行高 16px
  *  - 换行：'\n'（'\r' 忽略）、行满自动换行；超过行数后整体上移（滚动）
  ******************************************************************************
  */

#ifndef SERIAL_TERM_H
#define SERIAL_TERM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 行宽按"像素"计算（ASCII 步进 = 8 点阵 + 1 空隙 = 9px，汉字 = 15px，见 gui.c），
 * 一行 320px：最多 35 个 ASCII 或 21 个汉字；行缓冲按最密的汉字算留余量 */
#define TERM_W_PX     320u  /* 终端区可用宽度（像素） */
#define TERM_ASCII_W  9u    /* ASCII 步进 */
#define TERM_CJK_W    15u   /* 汉字步进 */
#define TERM_COLS     44u   /* 行缓冲容量（字节）：21 个汉字 = 42 字节 + 余量 */
#define TERM_ROWS     8u    /* 行数（接收显示区：8 x 16px = 128px，y=16~143） */
#define TERM_Y0       16u   /* 终端区起始 y（第 1 行状态栏 0~15 之下） */

/* 初始化：启动 USART3 中断接收（在 MX_USART3_UART_Init 之后调用） */
void    SerialTerm_Init(void);

/* 主循环周期调用：取出缓冲数据 -> 换行/滚动/重画。
 * 返回 1 = 收到关机命令 'o'/'O' */
uint8_t SerialTerm_Task(void);

/* 接收显示模式：0 = 文本(ASCII)，1 = HEX（每字节两位十六进制 + 空格）。
 * 切换时清空接收显示区重新开始 */
void    SerialTerm_SetHexMode(uint8_t hex);
uint8_t SerialTerm_IsHexMode(void);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_TERM_H */
