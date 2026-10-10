/**
  ******************************************************************************
  * @file    disp_font_flash.h
  * @brief   字库层：把 SPI2 上的字库 Flash 包装成图形库的字体（GUI_Font_t）
  *
  *  图形层不关心字模存在哪：内置点阵表、SPI 字库芯片、外部数组都是同一个接口。
  *  用法：
  *      Disp_FontFlash_Init();
  *      GUI_SetAsciiFont(&Disp_FontFlash_Ascii8x16);
  *      GUI_SetCjkFont(&Disp_FontFlash_Cjk24x24);
  ******************************************************************************
  */

#ifndef DISP_FONT_FLASH_H
#define DISP_FONT_FLASH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gui_font.h"
#include "font_lib.h"

/* 初始化 SPI2 字库（0 = 成功）。失败时各字体取字模都会返回错误，图形层自动跳过 */
uint8_t Disp_FontFlash_Init(void);
uint8_t Disp_FontFlash_IsReady(void);

/* ASCII 字体 */
extern const GUI_Font_t Disp_FontFlash_Ascii5x7;
extern const GUI_Font_t Disp_FontFlash_Ascii6x12;
extern const GUI_Font_t Disp_FontFlash_Ascii8x16;
extern const GUI_Font_t Disp_FontFlash_Ascii12x24;
extern const GUI_Font_t Disp_FontFlash_Ascii16x32;

/* 汉字字体（GB2312） */
extern const GUI_Font_t Disp_FontFlash_Cjk12x12;
extern const GUI_Font_t Disp_FontFlash_Cjk15x16;
extern const GUI_Font_t Disp_FontFlash_Cjk24x24;
extern const GUI_Font_t Disp_FontFlash_Cjk32x32;

#ifdef __cplusplus
}
#endif

#endif /* DISP_FONT_FLASH_H */
