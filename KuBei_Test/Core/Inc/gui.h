/**
  ******************************************************************************
  * @file    gui.h
  * @brief   图形库（与硬件解耦）—— 图元 + 文字
  *
  *  分层：
  *      应用层                gui.h / gui.c（本文件，纯逻辑，无任何寄存器操作）
  *        ↓ GUI_DispDrv_t
  *      显示驱动层            disp_xxx.c（如 disp_ili9327.c）
  *        ↓
  *      硬件层                lcd_ili9327.c（FSMC）+ 外挂字库 font_lib.c（SPI）
  *
  *  典型用法：
  *      GUI_Init(&Disp_ILI9327);                    // 绑定并初始化显示驱动
  *      GUI_SetCjkFont(&Disp_FontFlash_Cjk24x24);   // 汉字用 SPI 字库
  *      GUI_Clear(GUI_BLACK);
  *      GUI_DrawString(10, 10, "Hello", GUI_WHITE, GUI_BLACK, 1);
  *      GUI_FillCircle(60, 60, 20, GUI_RED);
  *
  *  所有坐标以"逻辑画面"左上角为原点，超出裁减区(默认整屏)的部分自动裁掉，
  *  驱动层收到的窗口永远是合法的，因此换屏时不必关心越界问题。
  ******************************************************************************
  */

#ifndef GUI_H
#define GUI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "gui_font.h"
#include "gui_disp.h"

/* ===================== 颜色 ===================== */
#define GUI_BLACK     0x0000u
#define GUI_WHITE     0xFFFFu
#define GUI_RED       0xF800u
#define GUI_GREEN     0x07E0u
#define GUI_BLUE      0x001Fu
#define GUI_YELLOW    0xFFE0u
#define GUI_CYAN      0x07FFu
#define GUI_MAGENTA   0xF81Fu
#define GUI_GRAY      0x8410u
#define GUI_DKBLUE    0x0008u
#define GUI_ORANGE    0xFC00u
/* RGB888 -> RGB565（方便直接用网页/绘图软件里的颜色值） */
#define GUI_RGB(r,g,b) ((uint16_t)((((uint16_t)(r) & 0xF8u) << 8) | \
                                   (((uint16_t)(g) & 0xFCu) << 3) | \
                                   (((uint16_t)(b) & 0xF8u) >> 3)))

/* ===================== 初始化 / 查询 ===================== */
uint8_t              GUI_Init(const GUI_DispDrv_t *drv);   /* 0 = 成功 */
const GUI_DispDrv_t *GUI_Driver(void);
uint16_t             GUI_Width(void);
uint16_t             GUI_Height(void);

/* ===================== 裁剪区 ===================== */
void GUI_SetClip(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void GUI_ResetClip(void);
void GUI_GetClip(int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1);

/* ===================== 基本图元 ===================== */
void GUI_Clear(uint16_t color);
void GUI_DrawPixel(int16_t x, int16_t y, uint16_t color);
void GUI_DrawHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void GUI_DrawVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
void GUI_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void GUI_FillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void GUI_DrawRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

void GUI_DrawCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void GUI_FillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void GUI_DrawEllipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color);
void GUI_FillEllipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color);
void GUI_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color);
void GUI_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color);
void GUI_DrawRoundRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t r, uint16_t color);
void GUI_FillRoundRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t r, uint16_t color);

/* 沿裁剪区（默认整屏）边缘画 thickness 像素的边框 */
void GUI_DrawFrame(int16_t thickness, uint16_t color);

/* 位图：pix 为行优先 RGB565 数组 */
void GUI_DrawImage(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint16_t *pix);

/* ===================== 文字 ===================== */
void              GUI_SetAsciiFont(const GUI_Font_t *font);   /* NULL = 用驱动内置 */
void              GUI_SetCjkFont(const GUI_Font_t *font);     /* NULL = 不支持汉字 */
const GUI_Font_t *GUI_AsciiFont(void);
const GUI_Font_t *GUI_CjkFont(void);

/* 直接画一个字模（自定义字模/图标也可以用它） */
void     GUI_DrawGlyphBuffer(int16_t x, int16_t y, const uint8_t *glyph,
                             uint16_t w, uint16_t h, uint8_t scan,
                             uint16_t fg, uint16_t bg, uint8_t scale);

/* 返回下一个字符的 x 坐标（即本字符占宽；画不下或取字模失败也返回推进后的坐标） */
uint16_t GUI_DrawChar(int16_t x, int16_t y, char ch, uint16_t fg, uint16_t bg, uint8_t scale);
uint16_t GUI_DrawString(int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t scale);
uint16_t GUI_DrawNumber(int16_t x, int16_t y, int32_t num, uint8_t len,
                        uint16_t fg, uint16_t bg, uint8_t scale);
uint16_t GUI_TextWidth(const char *str, uint8_t scale);
uint16_t GUI_CharWidth(void);
uint16_t GUI_CharHeight(void);

/* ===================== 背光 ===================== */
void GUI_SetBacklight(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* GUI_H */
