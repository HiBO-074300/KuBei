/**
  ******************************************************************************
  * @file    lcd_ili9327.c
  * @brief   ILI9327 16-bit 8080 并口屏驱动实现（FSMC Bank1/NE1）
  *
  *  初始化序列来源：阿莫论坛 3.2寸 400x240 ILI9327 模块实测可用代码
  *  （要点：0x11 退出睡眠后和初始化结束必须有长延时，否则屏不显示）
  ******************************************************************************
  */

#include "lcd_ili9327.h"        /* 内部已包含 lcd_ili9327_cfg.h（全部硬件配置） */

/* ================== 5x7 点阵 ASCII 字库 0x20 ~ 0x7E（每字符 5 字节，列式） ================== */
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 0x20 ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* 0x21 '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* 0x22 '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 0x23 '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 0x24 '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* 0x25 '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* 0x26 '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* 0x27 '\'' */
    {0x00,0x1C,0x22,0x41,0x00}, /* 0x28 '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* 0x29 ')' */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* 0x2A '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* 0x2B '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* 0x2C ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* 0x2D '-' */
    {0x00,0x60,0x60,0x00,0x00}, /* 0x2E '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* 0x2F '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0x30 '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* 0x31 '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* 0x32 '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* 0x33 '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* 0x34 '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* 0x35 '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 0x36 '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* 0x37 '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* 0x38 '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* 0x39 '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* 0x3A ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* 0x3B ';' */
    {0x00,0x08,0x14,0x22,0x41}, /* 0x3C '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* 0x3D '=' */
    {0x41,0x22,0x14,0x08,0x00}, /* 0x3E '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* 0x3F '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* 0x40 '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 0x41 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 0x42 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 0x43 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 0x44 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 0x45 'E' */
    {0x7F,0x09,0x09,0x01,0x01}, /* 0x46 'F' */
    {0x3E,0x41,0x41,0x51,0x32}, /* 0x47 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 0x48 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 0x49 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 0x4A 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 0x4B 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 0x4C 'L' */
    {0x7F,0x02,0x04,0x02,0x7F}, /* 0x4D 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 0x4E 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 0x4F 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 0x50 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 0x51 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 0x52 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 0x53 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 0x54 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 0x55 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 0x56 'V' */
    {0x7F,0x20,0x18,0x20,0x7F}, /* 0x57 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 0x58 'X' */
    {0x03,0x04,0x78,0x04,0x03}, /* 0x59 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 0x5A 'Z' */
    {0x00,0x00,0x7F,0x41,0x41}, /* 0x5B '[' */
    {0x02,0x04,0x08,0x10,0x20}, /* 0x5C '\' */
    {0x41,0x41,0x7F,0x00,0x00}, /* 0x5D ']' */
    {0x04,0x02,0x01,0x02,0x04}, /* 0x5E '^' */
    {0x40,0x40,0x40,0x40,0x40}, /* 0x5F '_' */
    {0x00,0x01,0x02,0x04,0x00}, /* 0x60 '`' */
    {0x20,0x54,0x54,0x54,0x78}, /* 0x61 'a' */
    {0x7F,0x48,0x44,0x44,0x38}, /* 0x62 'b' */
    {0x38,0x44,0x44,0x44,0x20}, /* 0x63 'c' */
    {0x38,0x44,0x44,0x48,0x7F}, /* 0x64 'd' */
    {0x38,0x54,0x54,0x54,0x18}, /* 0x65 'e' */
    {0x08,0x7E,0x09,0x01,0x02}, /* 0x66 'f' */
    {0x08,0x14,0x54,0x54,0x3C}, /* 0x67 'g' */
    {0x7F,0x08,0x04,0x04,0x78}, /* 0x68 'h' */
    {0x00,0x44,0x7D,0x40,0x00}, /* 0x69 'i' */
    {0x20,0x40,0x44,0x3D,0x00}, /* 0x6A 'j' */
    {0x00,0x7F,0x10,0x28,0x44}, /* 0x6B 'k' */
    {0x00,0x41,0x7F,0x40,0x00}, /* 0x6C 'l' */
    {0x7C,0x04,0x18,0x04,0x78}, /* 0x6D 'm' */
    {0x7C,0x08,0x04,0x04,0x78}, /* 0x6E 'n' */
    {0x38,0x44,0x44,0x44,0x38}, /* 0x6F 'o' */
    {0x7C,0x14,0x14,0x14,0x08}, /* 0x70 'p' */
    {0x08,0x14,0x14,0x18,0x7C}, /* 0x71 'q' */
    {0x7C,0x08,0x04,0x04,0x08}, /* 0x72 'r' */
    {0x48,0x54,0x54,0x54,0x20}, /* 0x73 's' */
    {0x04,0x3F,0x44,0x40,0x20}, /* 0x74 't' */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 0x75 'u' */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 0x76 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 0x77 'w' */
    {0x44,0x28,0x10,0x28,0x44}, /* 0x78 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 0x79 'y' */
    {0x44,0x64,0x54,0x4C,0x44}, /* 0x7A 'z' */
    {0x00,0x08,0x36,0x41,0x00}, /* 0x7B '{' */
    {0x00,0x00,0x7F,0x00,0x00}, /* 0x7C '|' */
    {0x00,0x41,0x36,0x08,0x00}, /* 0x7D '}' */
    {0x08,0x08,0x2A,0x1C,0x08}  /* 0x7E '~' */
};

/* ================== 底层 ================== */

/**
  * @brief  把 FSMC 时序改成适合 LCD 的速度
  *         CubeMX 默认给的是慢速 SRAM 时序(DataSetup=255)，刷屏会非常慢。
  *         推荐在 CubeMX 里直接改（Address setup=2, Data setup=5~10）。
  */
/**
  * @brief  把内置 5x7 ASCII 点阵表暴露给上层（显示驱动层需要把它适配成 GUI_Font_t）
  *         表结构：95 个字符（0x20~0x7E） x 5 字节，列式存放，每字节 bit0 在最上面
  */
const uint8_t *LCD_GetBuiltinFont5x7(void)
{
    return (const uint8_t *)font5x7;
}

static void LCD_FSMC_Timing(void)
{
    FSMC_NORSRAM_TimingTypeDef Timing;

    /* 数值含义见 lcd_ili9327_cfg.h 的 FSMC 时序参数表 */
    Timing.AddressSetupTime      = LCD_FSMC_ADDSET;
    Timing.AddressHoldTime       = LCD_FSMC_ADDHOLD;
    Timing.DataSetupTime         = LCD_FSMC_DATSET;
    Timing.BusTurnAroundDuration = LCD_FSMC_BTA;
    Timing.CLKDivision           = 0;
    Timing.DataLatency           = 0;
    Timing.AccessMode            = FSMC_ACCESS_MODE_A;

    HAL_SRAM_Init(&LCD_HSRAM, &Timing, NULL);
}

static void LCD_HardReset(void)
{
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(120);
}

static void LCD_WriteCmd(uint16_t cmd)
{
    LCD_REG = cmd;
}

static void LCD_WriteData(uint16_t d)
{
    LCD_RAM = d;
}

void LCD_WriteReg(uint16_t reg, uint16_t val)
{
    LCD_REG = reg;
    LCD_RAM = val;
}

uint16_t LCD_ReadReg(uint16_t reg)
{
    volatile uint16_t dummy;

    LCD_REG = reg;
    dummy = LCD_RAM;      /* ILI932x 读寄存器第一个数据常为无效值 */
    (void)dummy;
    return LCD_RAM;
}

/**
  * @brief  读 ID（寄存器 0x04，返回 ID1/ID2/ID3 的最后一个字节组合），仅诊断用
  */
uint16_t LCD_ReadID(void)
{
    volatile uint16_t a, b, c, d;

    LCD_REG = 0x0004;
    a = LCD_RAM;
    b = LCD_RAM;
    c = LCD_RAM;
    d = LCD_RAM;
    (void)a; (void)b; (void)c;
    return (uint16_t)d;
}

void LCD_WriteRAM_Prepare(void)
{
    LCD_REG = 0x002C;         /* Memory Write */
}

void LCD_WriteRAM(uint16_t color)
{
    LCD_RAM = color;
}

/**
  * @brief  设置 GRAM 窗口（0x2A 列地址 / 0x2B 行地址，各 4 个 8bit 参数）
  */
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint16_t a0, a1, b0, b1;

    if (x0 > x1) { uint16_t t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { uint16_t t = y0; y0 = y1; y1 = t; }

#if (LCD_SWAP_XY == 1)
    a0 = y0; a1 = y1;      /* 0x2A <- y */
    b0 = x0; b1 = x1;      /* 0x2B <- x */
#else
    a0 = x0; a1 = x1;      /* 0x2A <- x */
    b0 = y0; b1 = y1;      /* 0x2B <- y */
#endif

    LCD_REG = 0x002A;
    LCD_RAM = (uint16_t)(a0 >> 8);
    LCD_RAM = (uint16_t)(a0 & 0xFF);
    LCD_RAM = (uint16_t)(a1 >> 8);
    LCD_RAM = (uint16_t)(a1 & 0xFF);

    LCD_REG = 0x002B;
    LCD_RAM = (uint16_t)(b0 >> 8);
    LCD_RAM = (uint16_t)(b0 & 0xFF);
    LCD_RAM = (uint16_t)(b1 >> 8);
    LCD_RAM = (uint16_t)(b1 & 0xFF);

    LCD_REG = 0x002C;         /* 准备写 GRAM，窗口内地址自动递增 */
}

void LCD_SetCursor(uint16_t x, uint16_t y)
{
    LCD_SetWindow(x, y, x, y);
}

/* ================== 初始化序列（ILI9327 专用命令集） ================== */

void LCD_Init(void)
{
    LCD_FSMC_Timing();          /* 提速：CubeMX 默认时序太慢 */
    LCD_HardReset();

    LCD_WriteCmd(0x00E9);
    LCD_WriteData(0x0020);

    LCD_WriteCmd(0x0011);       /* Exit Sleep —— 退出睡眠后必须延时 */
    HAL_Delay(120);

    LCD_WriteCmd(0x00D1);       /* VCOM Control */
    LCD_WriteData(0x0000);
    LCD_WriteData(0x0071);
    LCD_WriteData(0x0019);

    LCD_WriteCmd(0x00D0);       /* Power Setting */
    LCD_WriteData(0x0007);
    LCD_WriteData(0x0001);
    LCD_WriteData(0x0008);
    HAL_Delay(20);

    LCD_WriteCmd(0x0036);       /* Memory Access Control（方向/BGR） */
    LCD_WriteData(LCD_MADCTL);

    LCD_WriteCmd(0x003A);       /* Interface Pixel Format: 16bit/pixel */
    LCD_WriteData(0x0005);

    LCD_WriteCmd(0x00C1);
    LCD_WriteData(0x0010);
    LCD_WriteData(0x0010);
    LCD_WriteData(0x0002);
    LCD_WriteData(0x0002);

    LCD_WriteCmd(0x00C0);       /* Panel Driving Setting */
    LCD_WriteData(0x0000);
    LCD_WriteData(0x0035);
    LCD_WriteData(0x0000);
    LCD_WriteData(0x0000);
    LCD_WriteData(0x0001);
    LCD_WriteData(0x0002);

    LCD_WriteCmd(0x00C5);       /* Frame Rate */
    LCD_WriteData(0x0004);

    LCD_WriteCmd(0x00D2);       /* Display Power */
    LCD_WriteData(0x0001);
    LCD_WriteData(0x0044);

    LCD_WriteCmd(0x00C8);       /* Gamma Setting */
    LCD_WriteData(0x0004);  LCD_WriteData(0x0067);  LCD_WriteData(0x0035);
    LCD_WriteData(0x0004);  LCD_WriteData(0x0008);  LCD_WriteData(0x0006);
    LCD_WriteData(0x0024);  LCD_WriteData(0x0001);  LCD_WriteData(0x0037);
    LCD_WriteData(0x0040);  LCD_WriteData(0x0003);  LCD_WriteData(0x0010);
    LCD_WriteData(0x0008);  LCD_WriteData(0x0080);  LCD_WriteData(0x0000);

    /* 整屏显示窗口（按 LCD_W/LCD_H 与 LCD_SWAP_XY 计算，和后续绘制保持一致） */
    LCD_SetWindow(0, 0, (uint16_t)(LCD_W - 1u), (uint16_t)(LCD_H - 1u));

    LCD_WriteCmd(0x0029);       /* Display ON */
    HAL_Delay(50);

    LCD_WriteCmd(0x002C);       /* Memory Write */
    HAL_Delay(100);             /* 实测结论：初始化结束后需要延时，否则不显示 */
}

/* ================== 基本绘图 ================== */

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_SetCursor(x, y);
    LCD_RAM = color;
}

void LCD_Clear(uint16_t color)
{
    uint32_t i;
    uint32_t total = (uint32_t)LCD_W * (uint32_t)LCD_H;

    LCD_SetWindow(0, 0, LCD_W - 1, LCD_H - 1);
    for (i = 0; i < total; i++)
    {
        LCD_RAM = color;
    }
}

/**
  * @brief  内部使用：带符号坐标的矩形填充，越界自动裁剪
  */
static void FillRect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color)
{
    uint32_t i;
    uint32_t total;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int32_t)LCD_W - 1) x1 = (int32_t)LCD_W - 1;
    if (y1 > (int32_t)LCD_H - 1) y1 = (int32_t)LCD_H - 1;
    if (x0 > x1 || y0 > y1) return;

    total = ((uint32_t)(x1 - x0) + 1u) * ((uint32_t)(y1 - y0) + 1u);
    LCD_SetWindow((uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1);
    for (i = 0; i < total; i++)
    {
        LCD_RAM = color;
    }
}

void LCD_Fill(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    FillRect((int32_t)x0, (int32_t)y0, (int32_t)x1, (int32_t)y1, color);
}

/**
  * @brief  整数平方根
  */
static uint32_t Isqrt(uint32_t v)
{
    uint32_t x, y;

    if (v == 0) return 0;
    x = v;
    y = (x + 1u) >> 1;
    while (y < x)
    {
        x = y;
        y = (x + v / x) >> 1;
    }
    return x;
}

/**
  * @brief  实心椭圆填充（逐行扫描）
  */
void LCD_FillEllipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color)
{
    int32_t dy, dx, t, ry2;

    if (rx <= 0 || ry <= 0) return;
    ry2 = (int32_t)ry * (int32_t)ry;

    for (dy = -(int32_t)ry; dy <= (int32_t)ry; dy++)
    {
        t = ry2 - dy * dy;
        dx = (int32_t)rx * (int32_t)Isqrt((uint32_t)t) / (int32_t)ry;
        FillRect((int32_t)cx - dx, (int32_t)cy + dy, (int32_t)cx + dx, (int32_t)cy + dy, color);
    }
}

/**
  * @brief  实心三角形填充（三点式扫描线，x 坐标放大 1000 倍做定点运算）
  */
void LCD_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color)
{
    int32_t xs, xe, xa, xb, xc, y, ymin, ymax;
    int16_t tx, ty;

    /* 按 y 从小到大排序三个顶点 */
    if (y0 > y1) { tx = x0; x0 = x1; x1 = tx; ty = y0; y0 = y1; y1 = ty; }
    if (y1 > y2) { tx = x1; x1 = x2; x2 = tx; ty = y1; y1 = y2; y2 = ty; }
    if (y0 > y1) { tx = x0; x0 = x1; x1 = tx; ty = y0; y0 = y1; y1 = ty; }

    ymin = y0;
    ymax = y2;
    if (ymax < 0 || ymin > (int32_t)LCD_H - 1) return;

    for (y = ymin; y <= ymax; y++)
    {
        /* 三条边在该行的交点（x*1000） */
        xa = (y1 != y0) ? ((int32_t)x0 * 1000 + ((int32_t)x1 - x0) * 1000 * (y - y0) / ((int32_t)y1 - y0))
                        : (int32_t)x0 * 1000;
        xb = (y2 != y0) ? ((int32_t)x0 * 1000 + ((int32_t)x2 - x0) * 1000 * (y - y0) / ((int32_t)y2 - y0))
                        : (int32_t)x0 * 1000;
        xc = (y2 != y1) ? ((int32_t)x1 * 1000 + ((int32_t)x2 - x1) * 1000 * (y - y1) / ((int32_t)y2 - y1))
                        : (int32_t)x1 * 1000;

        xs = xa; if (xb < xs) xs = xb; if (xc < xs) xs = xc;
        xe = xa; if (xb > xe) xe = xb; if (xc > xe) xe = xc;

        FillRect(xs / 1000, y, xe / 1000, y, color);
    }
}

/**
  * @brief  显示一张 RGB565 位图（行优先，左上角 (x,y)）
  */
void LCD_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pix)
{
    uint32_t i;
    uint32_t total;

    if (pix == NULL || w == 0 || h == 0) return;
    if (x + w > LCD_W || y + h > LCD_H) return;

    total = (uint32_t)w * (uint32_t)h;
    LCD_SetWindow(x, y, (uint16_t)(x + w - 1u), (uint16_t)(y + h - 1u));
    for (i = 0; i < total; i++)
    {
        LCD_RAM = pix[i];
    }
}

/**
  * @brief  清整片 GRAM（0 ~ LCD_GRAM_H-1 行）
  *         ILI9327 的 GRAM 是 240x432，而常见 3.2/4.0 寸面板只有 400 行可见；
  *         如果不把 400~431 这段也写一遍，面板上会露出没初始化过的"黑白条纹"。
  */
void LCD_ClearPanel(uint16_t color)
{
    uint32_t i;
    /* 432 x 432 的方块，两种坐标映射都能盖住整片 GRAM */
    uint32_t total = (uint32_t)LCD_GRAM_H * (uint32_t)LCD_GRAM_H;

    LCD_SetWindow(0, 0, (uint16_t)(LCD_GRAM_H - 1u), (uint16_t)(LCD_GRAM_H - 1u));
    for (i = 0; i < total; i++)
    {
        LCD_RAM = color;
    }
}

/**
  * @brief  沿 0 ~ LCD_W-1 / 0 ~ LCD_H-1 边缘画 thickness 像素宽的边框
  *         用来直观看出"显示区"和"可见玻璃区"是否重合
  */
void LCD_DrawFrame(uint16_t color, uint8_t thickness)
{
    uint8_t i;

    for (i = 0; i < thickness; i++)
    {
        LCD_DrawRect(i, i, (uint16_t)(LCD_W - 1u - i), (uint16_t)(LCD_H - 1u - i), color);
    }
}

void LCD_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    int16_t dx, dy, sx, sy, err, e2;

    dx = (x1 > x0) ? (int16_t)(x1 - x0) : (int16_t)(x0 - x1);
    dy = (y1 > y0) ? (int16_t)(y1 - y0) : (int16_t)(y0 - y1);
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = dx - dy;

    for (;;)
    {
        LCD_DrawPoint(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = err << 1;
        if (e2 > -dy) { err -= dy; x0 = (uint16_t)((int16_t)x0 + sx); }
        if (e2 <  dx) { err += dx; y0 = (uint16_t)((int16_t)y0 + sy); }
    }
}

void LCD_DrawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    LCD_DrawLine(x0, y0, x1, y0, color);
    LCD_DrawLine(x0, y1, x1, y1, color);
    LCD_DrawLine(x0, y0, x0, y1, color);
    LCD_DrawLine(x1, y0, x1, y1, color);
}

/* ================== 字符 / 字符串 ================== */

/**
  * @brief  显示一个字符（5x7 点阵，占 6x8 单元格），size=1/2/3... 为放大倍数
  */
void LCD_ShowChar(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg, uint8_t size)
{
    uint8_t c = (uint8_t)ch;
    uint8_t i, j, r, k;
    uint16_t color;

    if (c < 0x20 || c > 0x7E) c = '?';
    if (size == 0) size = 1;

    /* 开一个字符窗口，地址在窗口内自动递增，逐行写入即可 */
    LCD_SetWindow(x, y, (uint16_t)(x + 6u * size - 1u), (uint16_t)(y + 8u * size - 1u));

    for (j = 0; j < 8; j++)                 /* 行（第 8 行为空隙） */
    {
        for (r = 0; r < size; r++)          /* 行放大 */
        {
            for (i = 0; i < 5; i++)         /* 列 */
            {
                if (j < 7 && (font5x7[c - 0x20][i] & (1u << j)))
                    color = fg;
                else
                    color = bg;
                for (k = 0; k < size; k++)  /* 列放大 */
                {
                    LCD_RAM = color;
                }
            }
            for (k = 0; k < size; k++)      /* 字符右侧 1 列空隙 */
            {
                LCD_RAM = bg;
            }
        }
    }
}

void LCD_ShowString(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size)
{
    uint16_t cx = x;
    uint16_t step = (uint16_t)(6u * size);

    if (str == NULL) return;
    while (*str)
    {
        if (cx + step > LCD_W) break;       /* 超出屏宽则不再显示 */
        LCD_ShowChar(cx, y, *str, fg, bg, size);
        cx = (uint16_t)(cx + step);
        str++;
    }
}

void LCD_ShowInt(uint16_t x, uint16_t y, int32_t num, uint8_t len, uint16_t fg, uint16_t bg, uint8_t size)
{
    char buf[16];
    uint8_t i;
    uint8_t neg = 0;
    uint32_t n;

    if (len > 15) len = 15;
    if (num < 0) { neg = 1; n = (uint32_t)(-num); }
    else         { n = (uint32_t)num; }

    buf[len] = '\0';
    for (i = 0; i < len; i++)
    {
        buf[len - 1u - i] = (char)('0' + (n % 10u));
        n /= 10u;
    }
    if (neg && len > 0) buf[0] = '-';

    LCD_ShowString(x, y, buf, fg, bg, size);
}

/* ================== 背光 ================== */

void LCD_BL_Init(void)
{
    HAL_TIM_PWM_Start(LCD_BL_TIM, LCD_BL_CH);   /* 引脚/通道见 lcd_ili9327_cfg.h */
    LCD_BL_SetBrightness(LCD_BL_FREQ_DEF);
}

void LCD_BL_SetBrightness(uint8_t percent)
{
    uint32_t period;
    uint32_t pulse;

    if (percent > 100) percent = 100;

    period = (uint32_t)(LCD_BL_TIM->Instance->ARR);     /* 自动重装载值 */
    pulse  = (period + 1u) * (uint32_t)percent / 100u;
    __HAL_TIM_SET_COMPARE(LCD_BL_TIM, LCD_BL_CH, pulse);
}
