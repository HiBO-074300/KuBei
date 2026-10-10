/**
  ******************************************************************************
  * @file    gui_font.h
  * @brief   图形库 —— 字体抽象接口
  *
  *  图形层不关心字模从哪来（内置点阵表 / SPI 字库芯片 / 外部数组），
  *  只通过 GUI_Font_t 里的 get_glyph 回调取字模，做到字体来源可插拔。
  ******************************************************************************
  */

#ifndef GUI_FONT_H
#define GUI_FONT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 取模方向 */
#define GUI_FONT_SCAN_H   0u    /* 横向取模：每行 ceil(w/8) 字节，左->右，MSB 在左 */
#define GUI_FONT_SCAN_V   1u    /* 纵向取模：每列 ceil(h/8) 字节，上->下，MSB 在上 */

/* 一个字符的字模最多多少字节（32x32 = 128） */
#define GUI_FONT_MAX_BYTES  128u

/**
  * @brief  取字模回调
  * @param  code 字符编码：ASCII 传 0x20~0x7E；汉字传 GB2312 双字节（如 '字' = 0xD7D6）
  * @param  buf  输出缓冲，容量至少 GUI_FONT_MAX_BYTES
  * @retval 0 = 成功，其它 = 失败
  */
typedef uint8_t (*GUI_GetGlyphFn)(uint32_t code, uint8_t *buf);

/**
  * @brief  字体描述
  */
typedef struct
{
    uint16_t        width;      /* 点阵宽（像素） */
    uint16_t        height;     /* 点阵高（像素） */
    uint8_t         bytes;      /* 每个字模字节数 */
    uint8_t         scan;       /* GUI_FONT_SCAN_H / GUI_FONT_SCAN_V */
    GUI_GetGlyphFn  get_glyph;  /* 取字模回调，不可为空 */
} GUI_Font_t;

#ifdef __cplusplus
}
#endif

#endif /* GUI_FONT_H */
