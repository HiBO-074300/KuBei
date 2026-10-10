/**
  ******************************************************************************
  * @file    gui_disp.h
  * @brief   图形库 —— 显示设备驱动接口（图形层与硬件之间的唯一桥梁）
  *
  *  图形层 (gui.c) 只认这个结构体，不认识 FSMC / SPI / 控制器寄存器。
  *  换屏时只需再写一个驱动实现（例如 disp_ili9327.c 换成 disp_st7789.c），
  *  图形层、文字层、应用层代码一行都不用改。
  *
  *  驱动实现必须提供：set_window + write_pixel（其余可为 NULL）
  *      set_window(x0,y0,x1,y1)  设定矩形窗口，坐标已由图形层裁剪好，驱动无需再裁
  *      write_pixel(color)       在窗口内按光栅顺序连续写一个像素（行优先，宽优先）
  ******************************************************************************
  */

#ifndef GUI_DISP_H
#define GUI_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "gui_font.h"

typedef struct
{
    uint16_t width;       /* 逻辑宽（像素） */
    uint16_t height;      /* 逻辑高（像素） */

    /* 初始化（可空）：返回 0 = 成功。GUI_Init() 会调用 */
    uint8_t (*init)(void);

    /* 必填：设置窗口 */
    void    (*set_window)(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

    /* 必填：向当前窗口按顺序写一个像素 */
    void    (*write_pixel)(uint16_t color);

    /* 可空：读像素，不支持就置 NULL */
    uint16_t (*read_pixel)(void);

    /* 可空：背光亮度 0~100 */
    void    (*set_backlight)(uint8_t percent);

    /* 可空：该屏自带的内置 ASCII 字体（GUI_Init 时作为默认字体） */
    const GUI_Font_t *builtin_ascii_font;

} GUI_DispDrv_t;

#ifdef __cplusplus
}
#endif

#endif /* GUI_DISP_H */
