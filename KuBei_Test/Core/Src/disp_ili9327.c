/**
  ******************************************************************************
  * @file    disp_ili9327.c
  * @brief   ILI9327 显示驱动层：把 lcd_ili9327.c 的硬件操作包装成 GUI_DispDrv_t
  *
  *  这一层是"图形库"和"硬件"之间唯一的接缝：
  *      图形层只调用 set_window / write_pixel
  *      本层负责翻译成 LCD_SetWindow / 写 FSMC 数据口
  ******************************************************************************
  */

#include "disp_ili9327.h"
#include "lcd_ili9327.h"

/* ---- GUI_DispDrv_t 要求的接口 ---- */

static uint8_t Drv_Init(void)
{
    LCD_Init();          /* 控制器初始化（含 FSMC 提速、复位、初始化序列） */
    LCD_BL_Init();       /* 背光 PWM 启动（PB0 = TIM3_CH3） */
    return 0u;
}

static void Drv_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    LCD_SetWindow(x0, y0, x1, y1);
}

static void Drv_WritePixel(uint16_t color)
{
    LCD_RAM = color;     /* 已开好窗口，连续写即可，地址自动递增 */
}

static void Drv_SetBacklight(uint8_t percent)
{
    LCD_BL_SetBrightness(percent);
}

/* ---- 内置 5x7 ASCII 字体 ----
 * lcd_ili9327.c 里的点阵表是"列式"存放（每字符 5 字节，每字节一列，bit0 在上），
 * 图形库要求"行式"（每行 ceil(w/8) 字节，MSB 在左），这里做一次展开。 */
static uint8_t Builtin5x7_GetGlyph(uint32_t code, uint8_t *buf)
{
    const uint8_t *tab;
    uint8_t i, j;

    if (code < 0x20u || code > 0x7Eu) code = (uint32_t)'?';
    tab = LCD_GetBuiltinFont5x7() + (code - 0x20u) * 5u;

    for (j = 0; j < 7u; j++) buf[j] = 0u;
    for (i = 0; i < 5u; i++)                    /* 5 列 */
    {
        for (j = 0; j < 7u; j++)                /* 每列 7 位 -> 展开成 7 行 */
        {
            if ((tab[i] & (uint8_t)(1u << j)) != 0u)
            {
                buf[j] |= (uint8_t)(0x80u >> i);
            }
        }
    }
    return 0u;
}

static const GUI_Font_t s_builtin5x7 =
{
    5u, 7u, 7u, GUI_FONT_SCAN_H, Builtin5x7_GetGlyph
};

/* ---- 显示设备对象 ---- */
const GUI_DispDrv_t Disp_ILI9327 =
{
    LCD_W,                       /* width  */
    LCD_H,                       /* height */
    Drv_Init,                    /* init   */
    Drv_SetWindow,               /* set_window  */
    Drv_WritePixel,              /* write_pixel */
    NULL,                        /* read_pixel 暂不支持 */
    Drv_SetBacklight,            /* set_backlight */
    &s_builtin5x7                /* 默认 ASCII 字体 */
};

/* ---- 驱动扩展 ---- */
void Disp_ILI9327_ClearPanel(uint16_t color)
{
    LCD_ClearPanel(color);
}

uint16_t Disp_ILI9327_ReadID(void)
{
    return LCD_ReadID();
}
