/**
  ******************************************************************************
  * @file    gui.c
  * @brief   图形库实现（与硬件完全解耦：只调用 GUI_DispDrv_t 里的函数指针）
  ******************************************************************************
  */

#include "gui.h"

/* ---------------- 内部状态 ---------------- */
static const GUI_DispDrv_t *s_drv   = NULL;
static const GUI_Font_t    *s_ascii = NULL;
static const GUI_Font_t    *s_cjk   = NULL;
static int16_t s_cx0 = 0, s_cy0 = 0, s_cx1 = 0, s_cy1 = 0;

/* ---------------- 小工具 ---------------- */
static uint32_t GUI_Isqrt(uint32_t v)
{
    uint32_t x, y;

    if (v == 0u) return 0u;
    x = v;
    y = (x + 1u) >> 1;
    while (y < x) { x = y; y = (x + v / x) >> 1; }
    return x;
}

static uint8_t GUI_ClipRect(int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1)
{
    if (*x0 < s_cx0) *x0 = s_cx0;
    if (*y0 < s_cy0) *y0 = s_cy0;
    if (*x1 > s_cx1) *x1 = s_cx1;
    if (*y1 > s_cy1) *y1 = s_cy1;
    return (uint8_t)((*x0 <= *x1) && (*y0 <= *y1));
}

/* 直接按窗口连续写像素（坐标必须已经裁剪过） */
static void GUI_FillAreaRaw(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    uint32_t n = (uint32_t)(x1 - x0 + 1) * (uint32_t)(y1 - y0 + 1);

    s_drv->set_window((uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1);
    while (n-- > 0u)
    {
        s_drv->write_pixel(color);
    }
}

static void GUI_FillArea(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    if (s_drv == NULL) return;
    if (!GUI_ClipRect(&x0, &y0, &x1, &y1)) return;
    GUI_FillAreaRaw(x0, y0, x1, y1, color);
}

/* ---------------- 初始化 / 查询 ---------------- */
uint8_t GUI_Init(const GUI_DispDrv_t *drv)
{
    if (drv == NULL) return 1u;
    if (drv->set_window == NULL || drv->write_pixel == NULL) return 2u;

    s_drv   = drv;
    s_ascii = drv->builtin_ascii_font;   /* 默认用驱动自带字体 */
    s_cjk   = NULL;
    GUI_ResetClip();

    if (drv->init != NULL) return drv->init();
    return 0u;
}

const GUI_DispDrv_t *GUI_Driver(void) { return s_drv; }
uint16_t GUI_Width(void)  { return (s_drv != NULL) ? s_drv->width  : 0u; }
uint16_t GUI_Height(void) { return (s_drv != NULL) ? s_drv->height : 0u; }

/* ---------------- 裁剪区 ---------------- */
void GUI_SetClip(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (s_drv == NULL) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int16_t)s_drv->width  - 1) x1 = (int16_t)s_drv->width  - 1;
    if (y1 > (int16_t)s_drv->height - 1) y1 = (int16_t)s_drv->height - 1;
    if (x1 < x0 || y1 < y0)
    {
        s_cx0 = 0; s_cy0 = 0; s_cx1 = -1; s_cy1 = -1;   /* 空裁剪区 */
        return;
    }
    s_cx0 = x0; s_cy0 = y0; s_cx1 = x1; s_cy1 = y1;
}

void GUI_ResetClip(void)
{
    if (s_drv == NULL) return;
    s_cx0 = 0;
    s_cy0 = 0;
    s_cx1 = (int16_t)s_drv->width  - 1;
    s_cy1 = (int16_t)s_drv->height - 1;
}

void GUI_GetClip(int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1)
{
    if (x0 != NULL) *x0 = s_cx0;
    if (y0 != NULL) *y0 = s_cy0;
    if (x1 != NULL) *x1 = s_cx1;
    if (y1 != NULL) *y1 = s_cy1;
}

/* ---------------- 基本图元 ---------------- */
void GUI_Clear(uint16_t color)
{
    GUI_FillArea(s_cx0, s_cy0, s_cx1, s_cy1, color);
}

void GUI_DrawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (s_drv == NULL) return;
    if (x < s_cx0 || y < s_cy0 || x > s_cx1 || y > s_cy1) return;
    s_drv->set_window((uint16_t)x, (uint16_t)y, (uint16_t)x, (uint16_t)y);
    s_drv->write_pixel(color);
}

void GUI_DrawHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    if (w <= 0) return;
    GUI_FillArea(x, y, (int16_t)(x + w - 1), y, color);
}

void GUI_DrawVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    if (h <= 0) return;
    GUI_FillArea(x, y, x, (int16_t)(y + h - 1), color);
}

void GUI_FillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
    GUI_FillArea(x0, y0, x1, y1, color);
}

void GUI_DrawRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }

    GUI_DrawHLine(x0, y0, (int16_t)(x1 - x0 + 1), color);
    GUI_DrawHLine(x0, y1, (int16_t)(x1 - x0 + 1), color);
    GUI_DrawVLine(x0, y0, (int16_t)(y1 - y0 + 1), color);
    GUI_DrawVLine(x1, y0, (int16_t)(y1 - y0 + 1), color);
}

void GUI_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    int16_t dx, dy, sx, sy, err, e2;

    if (y0 == y1) { GUI_DrawHLine((x0 < x1) ? x0 : x1, y0, (int16_t)((x1 > x0 ? x1 - x0 : x0 - x1) + 1), color); return; }
    if (x0 == x1) { GUI_DrawVLine(x0, (y0 < y1) ? y0 : y1, (int16_t)((y1 > y0 ? y1 - y0 : y0 - y1) + 1), color); return; }

    dx = (x1 > x0) ? (int16_t)(x1 - x0) : (int16_t)(x0 - x1);
    dy = (y1 > y0) ? (int16_t)(y1 - y0) : (int16_t)(y0 - y1);
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = (int16_t)(dx - dy);

    for (;;)
    {
        GUI_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = (int16_t)(err << 1);
        if (e2 > -dy) { err = (int16_t)(err - dy); x0 = (int16_t)(x0 + sx); }
        if (e2 <  dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
}

void GUI_DrawFrame(int16_t thickness, uint16_t color)
{
    int16_t i;

    for (i = 0; i < thickness; i++)
    {
        GUI_DrawRect((int16_t)(s_cx0 + i), (int16_t)(s_cy0 + i),
                     (int16_t)(s_cx1 - i), (int16_t)(s_cy1 - i), color);
    }
}

void GUI_FillEllipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color)
{
    int32_t dy, dx, t, ry2;

    if (rx <= 0 || ry <= 0) return;
    ry2 = (int32_t)ry * (int32_t)ry;

    for (dy = -(int32_t)ry; dy <= (int32_t)ry; dy++)
    {
        t  = ry2 - dy * dy;
        dx = (int32_t)rx * (int32_t)GUI_Isqrt((uint32_t)t) / (int32_t)ry;
        GUI_FillArea((int16_t)(cx - dx), (int16_t)(cy + dy),
                     (int16_t)(cx + dx), (int16_t)(cy + dy), color);
    }
}

void GUI_FillCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
    GUI_FillEllipse(cx, cy, r, r, color);
}

static void GUI_EllipsePlot(int16_t cx, int16_t cy, int16_t x, int16_t y, uint16_t color)
{
    GUI_DrawPixel((int16_t)(cx + x), (int16_t)(cy + y), color);
    GUI_DrawPixel((int16_t)(cx - x), (int16_t)(cy + y), color);
    GUI_DrawPixel((int16_t)(cx + x), (int16_t)(cy - y), color);
    GUI_DrawPixel((int16_t)(cx - x), (int16_t)(cy - y), color);
}

void GUI_DrawEllipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color)
{
    int32_t x = 0, y = ry;
    int32_t rx2, ry2, d1, dx, dy;

    if (rx <= 0 || ry <= 0) return;

    rx2 = (int32_t)rx * rx;
    ry2 = (int32_t)ry * ry;
    d1  = ry2 - rx2 * ry + (rx2 / 4);
    dx  = 0;
    dy  = 2 * rx2 * y;

    /* 第一段：|斜率| < 1 */
    while ((dx < dy) && (y >= 0))
    {
        GUI_EllipsePlot(cx, cy, (int16_t)x, (int16_t)y, color);
        x++;
        dx += 2 * ry2;
        if (d1 < 0)
        {
            d1 += dx + ry2;
        }
        else
        {
            y--;
            dy -= 2 * rx2;
            d1 += dx - dy + ry2;
        }
    }

    /* 第二段：|斜率| >= 1 */
    d1 = ry2 * (x * x + x) + rx2 * ((y - 1) * (y - 1)) - rx2 * ry2 + rx2;
    while (y >= 0)
    {
        GUI_EllipsePlot(cx, cy, (int16_t)x, (int16_t)y, color);
        y--;
        dy -= 2 * rx2;
        if (d1 > 0)
        {
            d1 += rx2 - dy;
        }
        else
        {
            x++;
            dx += 2 * ry2;
            d1 += dx - dy + rx2;
        }
    }
}

void GUI_DrawCircle(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
    GUI_DrawEllipse(cx, cy, r, r, color);
}

void GUI_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color)
{
    int32_t a0, a1, a2, xs, xe, y;
    int16_t t;

    if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (y1 > y2) { t = x1; x1 = x2; x2 = t; t = y1; y1 = y2; y2 = t; }
    if (y0 > y1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }

    for (y = y0; y <= y2; y++)
    {
        a0 = (y1 != y0) ? ((int32_t)x0 * 1000 + ((int32_t)x1 - x0) * 1000 * (y - y0) / ((int32_t)y1 - y0))
                        : (int32_t)x0 * 1000;
        a1 = (y2 != y0) ? ((int32_t)x0 * 1000 + ((int32_t)x2 - x0) * 1000 * (y - y0) / ((int32_t)y2 - y0))
                        : (int32_t)x0 * 1000;
        a2 = (y2 != y1) ? ((int32_t)x1 * 1000 + ((int32_t)x2 - x1) * 1000 * (y - y1) / ((int32_t)y2 - y1))
                        : (int32_t)x1 * 1000;

        xs = a0; if (a1 < xs) xs = a1; if (a2 < xs) xs = a2;
        xe = a0; if (a1 > xe) xe = a1; if (a2 > xe) xe = a2;

        GUI_FillArea((int16_t)(xs / 1000), (int16_t)y, (int16_t)(xe / 1000), (int16_t)y, color);
    }
}

void GUI_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color)
{
    GUI_DrawLine(x0, y0, x1, y1, color);
    GUI_DrawLine(x1, y1, x2, y2, color);
    GUI_DrawLine(x2, y2, x0, y0, color);
}

void GUI_FillRoundRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t r, uint16_t color)
{
    int16_t yy, hw, rr;

    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
    if (r <= 0) { GUI_FillArea(x0, y0, x1, y1, color); return; }
    rr = (int16_t)(((x1 - x0) < (y1 - y0)) ? (x1 - x0) / 2 : (y1 - y0) / 2);
    if (r > rr) r = rr;
    if (r <= 0) { GUI_FillArea(x0, y0, x1, y1, color); return; }

    /* 中间横带 + 上下两段的圆角 */
    GUI_FillArea(x0, (int16_t)(y0 + r), x1, (int16_t)(y1 - r), color);

    for (yy = 0; yy < r; yy++)
    {
        hw = (int16_t)GUI_Isqrt((uint32_t)((int32_t)r * r - (int32_t)(r - yy) * (r - yy)));
        GUI_FillArea((int16_t)(x0 + r - hw), (int16_t)(y0 + yy),
                     (int16_t)(x1 - r + hw), (int16_t)(y0 + yy), color);
        GUI_FillArea((int16_t)(x0 + r - hw), (int16_t)(y1 - yy),
                     (int16_t)(x1 - r + hw), (int16_t)(y1 - yy), color);
    }
}

void GUI_DrawRoundRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t r, uint16_t color)
{
    int16_t yy, hw, px, py;
    int16_t rr;

    if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
    if (r <= 0) { GUI_DrawRect(x0, y0, x1, y1, color); return; }
    rr = (int16_t)(((x1 - x0) < (y1 - y0)) ? (x1 - x0) / 2 : (y1 - y0) / 2);
    if (r > rr) r = rr;
    if (r <= 0) { GUI_DrawRect(x0, y0, x1, y1, color); return; }

    GUI_DrawHLine((int16_t)(x0 + r), y0, (int16_t)(x1 - x0 - 2 * r + 1), color);
    GUI_DrawHLine((int16_t)(x0 + r), y1, (int16_t)(x1 - x0 - 2 * r + 1), color);
    GUI_DrawVLine(x0, (int16_t)(y0 + r), (int16_t)(y1 - y0 - 2 * r + 1), color);
    GUI_DrawVLine(x1, (int16_t)(y0 + r), (int16_t)(y1 - y0 - 2 * r + 1), color);

    /* 四个圆角：相邻行之间用线段连接，保证不断线（px/py 记录上一行的弧点） */
    px = 0;
    py = 0;
    for (yy = 0; yy <= r; yy++)
    {
        hw = (int16_t)GUI_Isqrt((uint32_t)((int32_t)r * r - (int32_t)(r - yy) * (r - yy)));
        GUI_DrawLine((int16_t)(x0 + r - px), (int16_t)(y0 + py),
                     (int16_t)(x0 + r - hw), (int16_t)(y0 + yy), color);
        GUI_DrawLine((int16_t)(x1 - r + px), (int16_t)(y0 + py),
                     (int16_t)(x1 - r + hw), (int16_t)(y0 + yy), color);
        GUI_DrawLine((int16_t)(x0 + r - px), (int16_t)(y1 - py),
                     (int16_t)(x0 + r - hw), (int16_t)(y1 - yy), color);
        GUI_DrawLine((int16_t)(x1 - r + px), (int16_t)(y1 - py),
                     (int16_t)(x1 - r + hw), (int16_t)(y1 - yy), color);
        px = hw;
        py = yy;
    }
}

void GUI_DrawImage(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint16_t *pix)
{
    uint32_t i;
    int16_t x0, y0;

    if (s_drv == NULL || pix == NULL || w == 0u || h == 0u) return;

    /* 简化处理：只支持完全落在裁剪区内的位图，避免窗口与数据错位 */
    x0 = (int16_t)(x + (int16_t)w - 1);
    y0 = (int16_t)(y + (int16_t)h - 1);
    if (x < s_cx0 || y < s_cy0 || x0 > s_cx1 || y0 > s_cy1) return;

    s_drv->set_window((uint16_t)x, (uint16_t)y, (uint16_t)x0, (uint16_t)y0);
    for (i = 0; i < (uint32_t)w * (uint32_t)h; i++)
    {
        s_drv->write_pixel(pix[i]);
    }
}

/* ---------------- 文字 ---------------- */
void GUI_SetAsciiFont(const GUI_Font_t *font) { s_ascii = font; }
void GUI_SetCjkFont(const GUI_Font_t *font)   { s_cjk   = font; }
const GUI_Font_t *GUI_AsciiFont(void)         { return s_ascii; }
const GUI_Font_t *GUI_CjkFont(void)           { return s_cjk; }

uint16_t GUI_CharWidth(void)  { return (s_ascii != NULL) ? s_ascii->width  : 0u; }
uint16_t GUI_CharHeight(void) { return (s_ascii != NULL) ? s_ascii->height : 0u; }

void GUI_DrawGlyphBuffer(int16_t x, int16_t y, const uint8_t *glyph,
                         uint16_t w, uint16_t h, uint8_t scan,
                         uint16_t fg, uint16_t bg, uint8_t scale)
{
    int16_t rowBytes, colBytes;
    int16_t x0, y0, x1, y1;
    int16_t xx, yy, i, j;
    uint16_t bit;

    if (s_drv == NULL || glyph == NULL || w == 0u || h == 0u) return;
    if (scale == 0u) scale = 1u;

    rowBytes = (int16_t)((w + 7u) / 8u);
    colBytes = (int16_t)((h + 7u) / 8u);

    x0 = x;
    y0 = y;
    x1 = (int16_t)(x + (int16_t)(w * scale) - 1);
    y1 = (int16_t)(y + (int16_t)(h * scale) - 1);
    if (!GUI_ClipRect(&x0, &y0, &x1, &y1)) return;

    s_drv->set_window((uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1);

    for (yy = y0; yy <= y1; yy++)
    {
        j = (int16_t)((yy - y) / scale);
        for (xx = x0; xx <= x1; xx++)
        {
            i = (int16_t)((xx - x) / scale);
            if (i >= (int16_t)w || j >= (int16_t)h)
            {
                s_drv->write_pixel(bg);
                continue;
            }
            if (scan == GUI_FONT_SCAN_H)
            {
                bit = (uint16_t)(glyph[(int32_t)j * rowBytes + (i >> 3)] & (0x80u >> (i & 7)));
            }
            else
            {
                bit = (uint16_t)(glyph[(int32_t)i * colBytes + (j >> 3)] & (0x80u >> (j & 7)));
            }
            s_drv->write_pixel((bit != 0u) ? fg : bg);
        }
    }
}

uint16_t GUI_DrawChar(int16_t x, int16_t y, char ch, uint16_t fg, uint16_t bg, uint8_t scale)
{
    uint8_t buf[GUI_FONT_MAX_BYTES];
    uint8_t sc = (scale == 0u) ? 1u : scale;

    if (s_ascii == NULL || s_ascii->get_glyph == NULL) return (uint16_t)x;

    if (s_ascii->get_glyph((uint32_t)(uint8_t)ch, buf) == 0u)
    {
        GUI_DrawGlyphBuffer(x, y, buf, s_ascii->width, s_ascii->height,
                            s_ascii->scan, fg, bg, sc);
    }
    return (uint16_t)(x + (int16_t)((s_ascii->width + 1u) * sc));   /* 右侧留 1 像素(乘放大倍数)间隙 */
}

uint16_t GUI_DrawString(int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t scale)
{
    const uint8_t *p = (const uint8_t *)str;
    uint8_t sc = (scale == 0u) ? 1u : scale;
    int16_t cx = x;
    int16_t asciiY = y;
    uint8_t buf[GUI_FONT_MAX_BYTES];

    if (str == NULL) return (uint16_t)x;

    /* 有汉字字体时，让 ASCII 与汉字底部对齐 */
    if (s_ascii != NULL && s_cjk != NULL && (s_cjk->height > s_ascii->height))
    {
        asciiY = (int16_t)(y + (int16_t)((s_cjk->height - s_ascii->height) * sc));
    }

    while (*p != 0u)
    {
        if (*p < 0x80u)                                  /* ASCII */
        {
            if (s_ascii != NULL && s_ascii->get_glyph != NULL)
            {
                if (s_ascii->get_glyph((uint32_t)(*p), buf) == 0u)
                {
                    GUI_DrawGlyphBuffer(cx, asciiY, buf, s_ascii->width, s_ascii->height,
                                        s_ascii->scan, fg, bg, sc);
                }
                cx = (int16_t)(cx + (int16_t)((s_ascii->width + 1u) * sc));
            }
            p++;
        }
        else                                             /* GB2312 双字节汉字 */
        {
            uint32_t code;

            if (p[1] == 0u) break;
            code = ((uint32_t)p[0] << 8) | (uint32_t)p[1];

            if (s_cjk != NULL && s_cjk->get_glyph != NULL)
            {
                if (s_cjk->get_glyph(code, buf) == 0u)
                {
                    GUI_DrawGlyphBuffer(cx, y, buf, s_cjk->width, s_cjk->height,
                                        s_cjk->scan, fg, bg, sc);
                }
                cx = (int16_t)(cx + (int16_t)(s_cjk->width * sc));
            }
            p += 2;
        }

        if (cx > s_cx1) break;
    }
    return (uint16_t)cx;
}

uint16_t GUI_DrawNumber(int16_t x, int16_t y, int32_t num, uint8_t len,
                        uint16_t fg, uint16_t bg, uint8_t scale)
{
    char buf[16];
    uint8_t i;
    uint8_t neg = 0;
    uint32_t n;

    if (len > 15u) len = 15u;
    if (num < 0) { neg = 1u; n = (uint32_t)(-num); }
    else         { n = (uint32_t)num; }

    buf[len] = '\0';
    for (i = 0; i < len; i++)
    {
        buf[len - 1u - i] = (char)('0' + (n % 10u));
        n /= 10u;
    }
    if (neg != 0u && len > 0u) buf[0] = '-';

    return GUI_DrawString(x, y, buf, fg, bg, scale);
}

uint16_t GUI_TextWidth(const char *str, uint8_t scale)
{
    const uint8_t *p = (const uint8_t *)str;
    uint8_t sc = (scale == 0u) ? 1u : scale;
    uint32_t w = 0;

    if (str == NULL) return 0u;

    while (*p != 0u)
    {
        if (*p < 0x80u)
        {
            if (s_ascii != NULL) w += (uint32_t)(s_ascii->width + 1u) * sc;
            p++;
        }
        else
        {
            if (p[1] == 0u) break;
            if (s_cjk != NULL) w += (uint32_t)s_cjk->width * sc;
            p += 2;
        }
    }
    return (uint16_t)w;
}

/* ---------------- 背光 ---------------- */
void GUI_SetBacklight(uint8_t percent)
{
    if (s_drv != NULL && s_drv->set_backlight != NULL)
    {
        s_drv->set_backlight(percent);
    }
}
