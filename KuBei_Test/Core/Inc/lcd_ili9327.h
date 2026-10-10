/**
  ******************************************************************************
  * @file    lcd_ili9327.h
  * @brief   ILI9327 16-bit 8080 并口屏驱动 —— 对外 API
  *
  *  重要：ILI9327 的寄存器集与 ILI9325/9328 完全不同！
  *        它采用 0x2A/0x2B 开窗 + 0x2C 连续写 GRAM 的方式（同 ILI9341 风格）。
  *
  *  ★ 硬件相关内容（引脚、FSMC、时序、分辨率、方向）全部在
  *    lcd_ili9327_cfg.h 中 —— 换硬件只改那个文件，本文件只有接口声明。
  ******************************************************************************
  */

#ifndef LCD_ILI9327_H
#define LCD_ILI9327_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lcd_ili9327_cfg.h"   /* ← 所有硬件配置（引脚/FSMC/时序/分辨率） */

/* ===================== 常用颜色 (RGB565) ===================== */
#define LCD_COLOR_WHITE     0xFFFF
#define LCD_COLOR_BLACK     0x0000
#define LCD_COLOR_RED       0xF800
#define LCD_COLOR_GREEN     0x07E0
#define LCD_COLOR_BLUE      0x001F
#define LCD_COLOR_YELLOW    0xFFE0
#define LCD_COLOR_CYAN      0x07FF
#define LCD_COLOR_MAGENTA   0xF81F
#define LCD_COLOR_GRAY      0x8410
#define LCD_COLOR_DKBLUE    0x0008
#define LCD_COLOR_ORANGE    0xFC00

/* ===================== API ===================== */
void     LCD_Init(void);
uint16_t LCD_ReadID(void);

void     LCD_WriteReg(uint16_t reg, uint16_t val);
uint16_t LCD_ReadReg(uint16_t reg);

void     LCD_SetCursor(uint16_t x, uint16_t y);
void     LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void     LCD_WriteRAM_Prepare(void);
void     LCD_WriteRAM(uint16_t color);

void     LCD_Clear(uint16_t color);          /* 只清排版区 0 ~ LCD_H-1 */
void     LCD_ClearPanel(uint16_t color);     /* 清整片 GRAM 0 ~ LCD_GRAM_H-1，消除未写入区域的条纹 */
void     LCD_DrawFrame(uint16_t color, uint8_t thickness);  /* 沿 LCD_W x LCD_H 边缘画边框 */
void     LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);
void     LCD_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void     LCD_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void     LCD_DrawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

/* 图形基元：实心椭圆 / 实心三角形 / 位图（RGB565 数组） */
void     LCD_FillEllipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color);
void     LCD_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          int16_t x2, int16_t y2, uint16_t color);
void     LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pix);

/* 内置 5x7 ASCII 点阵表（95 字符 x 5 字节，列式），给显示驱动层适配用 */
const uint8_t *LCD_GetBuiltinFont5x7(void);

void     LCD_ShowChar(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg, uint8_t size);
void     LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size);
void     LCD_ShowInt(uint16_t x, uint16_t y, int32_t num, uint8_t len, uint16_t fg, uint16_t bg, uint8_t size);

/* 背光：PB0 = TIM3_CH3 PWM */
void     LCD_BL_Init(void);
void     LCD_BL_SetBrightness(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* LCD_ILI9327_H */
