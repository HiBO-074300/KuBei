/**
  ******************************************************************************
  * @file    disp_ili9327.h
  * @brief   显示驱动层：把 ILI9327 + FSMC 这块屏包装成图形库的显示设备
  *
  *  图形层只认识 GUI_DispDrv_t；本文件是"ILI9327 版"的实现。
  *  换屏时照着这个文件再写一份（例如 disp_st7789.c），上层代码不用改。
  ******************************************************************************
  */

#ifndef DISP_ILI9327_H
#define DISP_ILI9327_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gui_disp.h"

/* 传给 GUI_Init() 的显示设备对象（内置 5x7 ASCII 字体作为默认字体） */
extern const GUI_DispDrv_t Disp_ILI9327;

/* ---- 本驱动特有的扩展（不属于图形库通用接口） ---- */

/* 清整片 GRAM（含面板看不见的多余行），消除边缘未初始化造成的条纹 */
void     Disp_ILI9327_ClearPanel(uint16_t color);

/* 读控制器 ID（诊断用） */
uint16_t Disp_ILI9327_ReadID(void);

#ifdef __cplusplus
}
#endif

#endif /* DISP_ILI9327_H */
