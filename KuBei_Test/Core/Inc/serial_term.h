/**
  ******************************************************************************
  * @file    serial_term.h
  * @brief   串口终端：USART3(9600, PC10/PC11) 中断接收，屏上滚动显示
  *
  *  - 接收走中断 + 环形缓冲（主循环刷屏可能超过 9600 的字节间隔，轮询会丢字节）
  *  - 显示模式：文本(ASCII) / HEX（每字节两位十六进制），可随时切换
  *  - 换行：'\n'（'\r' 忽略）、行满自动换行；超过行数后整体上移（滚动）
  ******************************************************************************
  */

#ifndef SERIAL_TERM_H
#define SERIAL_TERM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define TERM_COLS   53u    /* 每行字符数（320px / 内置5x7字体 6px） */
#define TERM_ROWS   17u    /* 行数（接收显示区占最大空间：17 x 8px = 136px） */
#define TERM_Y0     16u    /* 终端区起始 y（第 1 行状态栏 0~15 之下） */

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
