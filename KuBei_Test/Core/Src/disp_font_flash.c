/**
  ******************************************************************************
  * @file    disp_font_flash.c
  * @brief   字库 Flash(SPI2) -> GUI_Font_t 适配实现
  *
  *  每种点阵一个取字模回调，内部转调 font_lib.c 的 Font_ReadAscii / Font_ReadChinese。
  *  取模方向按字库芯片的实际情况设定（横向取模见 font_lib.h 的说明）。
  ******************************************************************************
  */

#include "disp_font_flash.h"

/* 字库芯片是横向取模、字节正序；若换成纵向取模的字库，把这里改成 GUI_FONT_SCAN_V */
#define FONT_FLASH_SCAN   GUI_FONT_SCAN_H

/* ---------------- ASCII ---------------- */
static uint8_t GetAscii5x7(uint32_t code, uint8_t *buf)
{
    return (Font_ReadAscii((uint8_t)code, buf, CHAR_SIZE_5x7) == FONT_OK) ? 0u : 1u;
}
static uint8_t GetAscii6x12(uint32_t code, uint8_t *buf)
{
    return (Font_ReadAscii((uint8_t)code, buf, CHAR_SIZE_6x12) == FONT_OK) ? 0u : 1u;
}
static uint8_t GetAscii8x16(uint32_t code, uint8_t *buf)
{
    return (Font_ReadAscii((uint8_t)code, buf, CHAR_SIZE_8x16) == FONT_OK) ? 0u : 1u;
}
static uint8_t GetAscii12x24(uint32_t code, uint8_t *buf)
{
    return (Font_ReadAscii((uint8_t)code, buf, CHAR_SIZE_12x24) == FONT_OK) ? 0u : 1u;
}
static uint8_t GetAscii16x32(uint32_t code, uint8_t *buf)
{
    return (Font_ReadAscii((uint8_t)code, buf, CHAR_SIZE_16x32) == FONT_OK) ? 0u : 1u;
}

const GUI_Font_t Disp_FontFlash_Ascii5x7   = {  5u,  7u,  8u, FONT_FLASH_SCAN, GetAscii5x7  };
const GUI_Font_t Disp_FontFlash_Ascii6x12  = {  6u, 12u, 12u, FONT_FLASH_SCAN, GetAscii6x12 };
const GUI_Font_t Disp_FontFlash_Ascii8x16  = {  8u, 16u, 16u, FONT_FLASH_SCAN, GetAscii8x16 };
const GUI_Font_t Disp_FontFlash_Ascii12x24 = { 12u, 24u, 48u, FONT_FLASH_SCAN, GetAscii12x24 };
const GUI_Font_t Disp_FontFlash_Ascii16x32 = { 16u, 32u, 64u, FONT_FLASH_SCAN, GetAscii16x32 };

/* ---------------- 汉字 ---------------- */
static uint8_t GetCjk12x12(uint32_t code, uint8_t *buf)
{
    return (Font_ReadChinese((uint16_t)code, buf, FONT_SIZE_12x12) == FONT_OK) ? 0u : 1u;
}
static uint8_t GetCjk15x16(uint32_t code, uint8_t *buf)
{
    return (Font_ReadChinese((uint16_t)code, buf, FONT_SIZE_15x16) == FONT_OK) ? 0u : 1u;
}
static uint8_t GetCjk24x24(uint32_t code, uint8_t *buf)
{
    return (Font_ReadChinese((uint16_t)code, buf, FONT_SIZE_24x24) == FONT_OK) ? 0u : 1u;
}
static uint8_t GetCjk32x32(uint32_t code, uint8_t *buf)
{
    return (Font_ReadChinese((uint16_t)code, buf, FONT_SIZE_32x32) == FONT_OK) ? 0u : 1u;
}

const GUI_Font_t Disp_FontFlash_Cjk12x12 = { 12u, 12u,  24u, FONT_FLASH_SCAN, GetCjk12x12 };
const GUI_Font_t Disp_FontFlash_Cjk15x16 = { 15u, 16u,  32u, FONT_FLASH_SCAN, GetCjk15x16 };
const GUI_Font_t Disp_FontFlash_Cjk24x24 = { 24u, 24u,  72u, FONT_FLASH_SCAN, GetCjk24x24 };
const GUI_Font_t Disp_FontFlash_Cjk32x32 = { 32u, 32u, 128u, FONT_FLASH_SCAN, GetCjk32x32 };

/* ---------------- 初始化 ---------------- */
uint8_t Disp_FontFlash_Init(void)
{
    return (Font_Init() == FONT_OK) ? 0u : 1u;
}

uint8_t Disp_FontFlash_IsReady(void)
{
    return Font_IsReady();
}
