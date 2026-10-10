/**
  ******************************************************************************
  * @file    font_lib.c
  * @brief   SPI2 上的字库 Flash 读取实现（GB2312 汉字 + ASCII）
  *
  *  设计：地址表驱动 —— 每种点阵只描述"基地址 / 每字模字节数 / 宽 / 高"，
  *        剩余流程（区位码换算 → 拼地址 → 0x03 读数据）全部共用一套代码。
  ******************************************************************************
  */

#include "font_lib.h"
#include "spi.h"
#include <string.h>

/* ---------------- 点阵描述表 ---------------- */
typedef struct
{
    uint32_t base;        /* 该点阵在字库 Flash 中的起始地址 */
    uint8_t  bytes;       /* 每个字模字节数 */
    uint8_t  width;       /* 点阵宽（像素） */
    uint8_t  height;      /* 点阵高（像素） */
} FontGlyphDesc;

/* 汉字（GB2312）点阵 */
static const FontGlyphDesc s_cjk[FONT_SIZE_COUNT] = {
    { 0x000000u,  24u, 12u, 12u },   /* FONT_SIZE_12x12 */
    { 0x02C9D0u,  32u, 15u, 16u },   /* FONT_SIZE_15x16 */
    { 0x068190u,  72u, 24u, 24u },   /* FONT_SIZE_24x24 */
    { 0x0EDF00u, 128u, 32u, 32u },   /* FONT_SIZE_32x32 */
    { 0x068190u,  72u, 24u, 24u }    /* FONT_SIZE_48x48：字库中暂无，先按 24x24 处理 */
};

/* ASCII 点阵 */
static const FontGlyphDesc s_ascii[CHAR_SIZE_COUNT] = {
    { 0x1DBE00u, 12u,  6u, 12u },    /* CHAR_SIZE_6x12  */
    { 0x1DD780u, 16u,  8u, 16u },    /* CHAR_SIZE_8x16  */
    { 0x1DFF00u, 48u, 12u, 24u },    /* CHAR_SIZE_12x24 */
    { 0x1E5A50u, 64u, 16u, 32u },    /* CHAR_SIZE_16x32 */
    { 0x1DFF00u, 48u, 12u, 24u },    /* CHAR_SIZE_24x48：字库中暂无，先按 12x24 处理 */
    { 0x1DDF80u,  8u,  5u,  7u },    /* CHAR_SIZE_5x7   */
    { 0x1DE280u,  8u,  7u,  8u }     /* CHAR_SIZE_7x8   */
};

static SpiFlash_t s_font;
static uint8_t    s_ready = 0;
static uint32_t   s_id    = 0;

/* ---------------- 内部工具 ---------------- */

/* 从字库 Flash 读一段数据 */
static uint8_t Font_ReadData(uint32_t addr, uint8_t *buf, uint32_t len)
{
    if (!s_ready) return FONT_ERR_NOTINIT;
    if (buf == NULL || len == 0u) return FONT_ERR_PARAM;
    if ((addr + len) > (FONT_FLASH_SIZE_KB * 1024u)) return FONT_ERR_RANGE;

    return (SpiFlash_Read(&s_font, addr, buf, len) == SPIFLASH_OK) ? FONT_OK : FONT_ERR_FLASH;
}

/* 取点阵描述，顺便做越界检查 */
static const FontGlyphDesc *Font_DescCjk(FontSize size)
{
    if ((uint32_t)size >= (uint32_t)FONT_SIZE_COUNT) return NULL;
    return &s_cjk[(uint32_t)size];
}

static const FontGlyphDesc *Font_DescAscii(CharSize size)
{
    if ((uint32_t)size >= (uint32_t)CHAR_SIZE_COUNT) return NULL;
    return &s_ascii[(uint32_t)size];
}

/**
  * @brief  GB2312 编码 → 字模序号
  *         0xA1A1~0xA9FE 为符号区（1~9 区），0xB0A1~0xF7FE 为汉字区（16~87 区）
  */
static uint8_t Font_Gb2312Index(uint8_t msb, uint8_t lsb, uint32_t *index)
{
    if (lsb < 0xA1u || lsb > 0xFEu) return FONT_ERR_PARAM;

    if (msb >= 0xA1u && msb <= 0xA9u)
    {
        *index = ((uint32_t)(msb - 0xA1u) * 94u) + (uint32_t)(lsb - 0xA1u);
        return FONT_OK;
    }
    if (msb >= 0xB0u && msb <= 0xF7u)
    {
        *index = ((uint32_t)(msb - 0xB0u) * 94u) + (uint32_t)(lsb - 0xA1u) + 846u;
        return FONT_OK;
    }
    return FONT_ERR_PARAM;
}

/* ---------------- 初始化 ---------------- */

SpiFlash_t *Font_Handle(void)
{
    return &s_font;
}

uint8_t Font_IsReady(void)
{
    return s_ready;
}

uint32_t Font_ReadFlashID(void)
{
    s_id = SpiFlash_ReadJedecID(&s_font);
    return s_id;
}

uint8_t Font_Init(void)
{
    uint8_t buf[16];
    uint8_t i;
    uint8_t allZero = 1;
    uint8_t allFF   = 1;

    /* 先允许底层读取（Font_ReadData 会检查 s_ready） */
    SpiFlash_Attach(&s_font, &hspi2, FLASH2_CS_GPIO_Port, FLASH2_CS_Pin, FONT_FLASH_SIZE_KB);
    s_ready = 1u;

    s_id = SpiFlash_ReadJedecID(&s_font);

    /* 用 8x16 的 'A' 做有效性检查：既不是全 0 也不是全 0xFF 才算字库正常 */
    if (Font_ReadAscii('A', buf, CHAR_SIZE_8x16) != FONT_OK)
    {
        s_ready = 0u;
        return FONT_ERR_FLASH;
    }
    for (i = 0; i < 16u; i++)
    {
        if (buf[i] != 0x00u) allZero = 0;
        if (buf[i] != 0xFFu) allFF   = 0;
    }
    if (allZero || allFF)
    {
        s_ready = 0u;
        return FONT_ERR_FLASH;
    }

    return FONT_OK;
}

/* ---------------- 尺寸查询 ---------------- */

uint8_t Font_GlyphBytes(FontSize size)
{
    const FontGlyphDesc *d = Font_DescCjk(size);
    return (d != NULL) ? d->bytes : 0u;
}

uint8_t Font_GlyphWidth(FontSize size)
{
    const FontGlyphDesc *d = Font_DescCjk(size);
    return (d != NULL) ? d->width : 0u;
}

uint8_t Font_GlyphHeight(FontSize size)
{
    const FontGlyphDesc *d = Font_DescCjk(size);
    return (d != NULL) ? d->height : 0u;
}

uint8_t Font_GlyphRowBytes(FontSize size)
{
    const FontGlyphDesc *d = Font_DescCjk(size);
    return (d != NULL) ? (uint8_t)((d->width + 7u) / 8u) : 0u;
}

uint8_t Font_CharBytes(CharSize size)
{
    const FontGlyphDesc *d = Font_DescAscii(size);
    return (d != NULL) ? d->bytes : 0u;
}

uint8_t Font_CharWidth(CharSize size)
{
    const FontGlyphDesc *d = Font_DescAscii(size);
    return (d != NULL) ? d->width : 0u;
}

uint8_t Font_CharHeight(CharSize size)
{
    const FontGlyphDesc *d = Font_DescAscii(size);
    return (d != NULL) ? d->height : 0u;
}

uint8_t Font_CharRowBytes(CharSize size)
{
    const FontGlyphDesc *d = Font_DescAscii(size);
    return (d != NULL) ? (uint8_t)((d->width + 7u) / 8u) : 0u;
}

/* ---------------- 读字模 ---------------- */

uint8_t Font_ReadChinese(uint16_t gb, uint8_t *buf, FontSize size)
{
    const FontGlyphDesc *d = Font_DescCjk(size);
    uint32_t index = 0;
    uint32_t addr;
    uint8_t  ret;

    if (d == NULL) return FONT_ERR_RANGE;
    if (buf == NULL) return FONT_ERR_PARAM;

    ret = Font_Gb2312Index((uint8_t)(gb >> 8), (uint8_t)(gb & 0xFF), &index);
    if (ret != FONT_OK) return ret;

    addr = d->base + index * (uint32_t)d->bytes;
    return Font_ReadData(addr, buf, d->bytes);
}

uint8_t Font_ReadAscii(uint8_t ch, uint8_t *buf, CharSize size)
{
    const FontGlyphDesc *d = Font_DescAscii(size);
    uint32_t addr;

    if (d == NULL) return FONT_ERR_RANGE;
    if (buf == NULL) return FONT_ERR_PARAM;
    if (ch < 0x20u || ch > 0x7Eu) return FONT_ERR_PARAM;

    addr = d->base + (uint32_t)(ch - 0x20u) * (uint32_t)d->bytes;
    return Font_ReadData(addr, buf, d->bytes);
}

/* 不做区位码范围检查，直接按 GB2312 索引读（给自制符号区扩展用） */
uint8_t Font_ReadChineseRaw(uint16_t gb, uint8_t *buf, FontSize size)
{
    const FontGlyphDesc *d = Font_DescCjk(size);
    uint8_t  msb = (uint8_t)(gb >> 8);
    uint8_t  lsb = (uint8_t)(gb & 0xFF);
    uint32_t index;
    uint32_t addr;

    if (d == NULL || buf == NULL) return FONT_ERR_PARAM;

    if (lsb < 0xA1u) lsb = 0xA1u;
    if (msb < 0xA1u) msb = 0xA1u;

    if (msb <= 0xA9u)
        index = ((uint32_t)(msb - 0xA1u) * 94u) + (uint32_t)(lsb - 0xA1u);
    else
        index = ((uint32_t)(msb - 0xB0u) * 94u) + (uint32_t)(lsb - 0xA1u) + 846u;

    addr = d->base + index * (uint32_t)d->bytes;
    return Font_ReadData(addr, buf, d->bytes);
}

/* ---------------- 兼容原接口 ---------------- */

FontSize Read_One_Font(uint16_t addr, uint8_t *data, FontSize datasize)
{
    if (Font_ReadChinese(addr, data, datasize) != FONT_OK) return (FontSize)FONT_ERR_VALUE;
    return datasize;
}

CharSize Read_One_Char(uint8_t addr, uint8_t *data, CharSize datasize)
{
    if (Font_ReadAscii(addr, data, datasize) != FONT_OK) return (CharSize)FONT_ERR_VALUE;
    return datasize;
}
