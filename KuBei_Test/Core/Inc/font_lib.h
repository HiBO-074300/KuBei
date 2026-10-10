/**
  ******************************************************************************
  * @file    font_lib.h
  * @brief   SPI2 上的字库 Flash（GB2312 汉字 + ASCII 点阵）读取驱动
  *
  *  硬件：SPI2 = PB13/PB14/PB15，片选 FLASH2_CS = PB12（见 main.h）
  *
  *  字库布局（沿用原 Fontlib.c 中的地址表，这也是 GT20L16S1Y 系列字库芯片的通用布局）：
  *
  *    类型        基地址      每字模字节数   说明
  *    --------------------------------------------------------------
  *    12x12 汉字  0x000000    24             GB2312 全部汉字/符号
  *    15x16 汉字  0x02C9D0    32
  *    24x24 汉字  0x068190    72
  *    32x32 汉字  0x0EDF00    128
  *    6x12  ASCII 0x1DBE00    12
  *    8x16  ASCII 0x1DD780    16
  *    5x7   ASCII 0x1DDF80    8
  *    7x8   ASCII 0x1DE280    8
  *    12x24 ASCII 0x1DFF00    48
  *    16x32 ASCII 0x1E5A50    64
  *
  *  与原来 Fontlib.c 相比的改动（原文件保留不动）：
  *    1) 片选改成 main.h 里真实存在的 FLASH2_CS(PB12)，SPI 改成 SPI2（原来写死 hspi1 且用了不存在的 CS_Pin）；
  *    2) 去掉对 word_stock.h / tab_askii / img2_24 等本地表的依赖（这些文件工程里没有，导致编译不过），
  *       所有字模统一从字库 Flash 读取；
  *    3) 地址表改为查表，五种"发命令+发地址+读数据"的重复代码合成一个函数；
  *    4) 补上参数校验（区位码范围、ASCII 范围、地址越界），返回值统一 0 = 成功；
  *    5) 增加字模尺寸查询接口，方便显示端按实际点阵宽度排版。
  ******************************************************************************
  */

#ifndef FONT_LIB_H
#define FONT_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "spi_flash.h"

/* ---------------- 字库 Flash 参数 ---------------- */
#define FONT_FLASH_SIZE_KB    2048u    /* GT20L16 系列为 16Mbit = 2MB */

/* ---------------- 点阵类型（枚举值与原 Fontlib.h 保持一致） ---------------- */
typedef enum {
    FONT_SIZE_12x12 = 0,
    FONT_SIZE_15x16,
    FONT_SIZE_24x24,
    FONT_SIZE_32x32,
    FONT_SIZE_48x48,
    FONT_SIZE_COUNT
} FontSize;

typedef enum {
    CHAR_SIZE_6x12 = 0,
    CHAR_SIZE_8x16,
    CHAR_SIZE_12x24,
    CHAR_SIZE_16x32,
    CHAR_SIZE_24x48,
    CHAR_SIZE_5x7,
    CHAR_SIZE_7x8,
    CHAR_SIZE_COUNT
} CharSize;

/* ---------------- 返回值 ---------------- */
#define FONT_OK           0u
#define FONT_ERR_PARAM    1u    /* 参数或字符编码非法 */
#define FONT_ERR_RANGE    2u    /* 字库类型越界 */
#define FONT_ERR_FLASH    3u    /* SPI/Flash 读失败 */
#define FONT_ERR_NOTINIT  4u    /* 未初始化 */
#define FONT_ERR_VALUE    0xFFu /* 与原 Fontlib 兼容：失败时的返回值 */

/* ---------------- 初始化与状态 ---------------- */
SpiFlash_t *Font_Handle(void);
uint8_t     Font_Init(void);            /* 0 = 成功（片选/SPI 就绪且字库能读出有效数据） */
uint8_t     Font_IsReady(void);
uint32_t    Font_ReadFlashID(void);     /* 字库芯片的 JEDEC ID，只做信息打印（部分字库芯片不支持 0x9F） */

/* ---------------- 字模尺寸查询 ---------------- */
uint8_t     Font_GlyphBytes(FontSize size);     /* 每个汉字字模占多少字节 */
uint8_t     Font_GlyphWidth(FontSize size);
uint8_t     Font_GlyphHeight(FontSize size);
uint8_t     Font_CharBytes(CharSize size);      /* 每个 ASCII 字模占多少字节 */
uint8_t     Font_CharWidth(CharSize size);
uint8_t     Font_CharHeight(CharSize size);
uint8_t     Font_GlyphRowBytes(FontSize size);  /* 每行占多少字节 = (宽+7)/8 */
uint8_t     Font_CharRowBytes(CharSize size);

/* ---------------- 读字模 ---------------- */
/* 读一个汉字：gb 为 GB2312 双字节编码，buf 需 >= Font_GlyphBytes(size) */
uint8_t     Font_ReadChinese(uint16_t gb, uint8_t *buf, FontSize size);

/* 读一个 ASCII 字符：ch 为 0x20~0x7E，buf 需 >= Font_CharBytes(size) */
uint8_t     Font_ReadAscii(uint8_t ch, uint8_t *buf, CharSize size);

/* 按给定 GB2312 码直接读（不做范围检查，给特殊符号用） */
uint8_t     Font_ReadChineseRaw(uint16_t gb, uint8_t *buf, FontSize size);

/* ---------------- 与原 Fontlib.c 同名的兼容接口（返回点阵类型，失败返回 0xFF） ---------------- */
FontSize    Read_One_Font(uint16_t addr, uint8_t *data, FontSize datasize);
CharSize    Read_One_Char(uint8_t addr, uint8_t *data, CharSize datasize);

#ifdef __cplusplus
}
#endif

#endif /* FONT_LIB_H */
