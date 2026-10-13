/**
  ******************************************************************************
  * @file    send_panel.c
  * @brief   发送面板实现：软键盘光标选择 + 发送框 + USART3 发送
  ******************************************************************************
  */

#include "send_panel.h"
#include "usart.h"
#include "gui.h"
#include "disp_font_flash.h"
#include "tm1668.h"
#include "serial_term.h"
#include <string.h>

/* ---------------- 功能键标记（非可输入字符） ---------------- */
#define KEY_SP    0x01u     /* 空格 */
#define KEY_DEL   0x02u     /* 退格 */
#define KEY_TXSW  0x03u     /* 切换发送模式 TXT/HEX */
#define KEY_RXSW  0x04u     /* 切换接收显示模式 TXT/HEX */
#define KEY_SHIFT 0x05u     /* 大小写切换 */

/* ---------------- 布局（QWERTY 软键盘占屏幕底部，发送框两行紧贴其上） ---------------- */
#define PANEL_ROWS  4u
#define PANEL_COLS  11u
#define PANEL_Y0    184u    /* 软键盘起始 y（4 行 x 14px = 56px，到 y=239 屏底） */
#define CELL_W      29u     /* 格宽 320/11 */
#define CELL_H      14u     /* 格高：6x12 字符(12px) + 2px，字符撑满格子 */
#define TXBOX_Y     152u    /* 发送框 y（2 行 x 16px，接收区 y=16~151） */
#define TXBOX_ROWS  2u      /* 发送框行数：内容超一行自动换行显示 */
#define TX_COLS     34u     /* 发送框每行可显示字符数（(320-12)/9） */
#define TX_MAX      64u     /* 发送框最多字符数 */
#define CHAR_W      9u      /* 8x16 字体步进（发送框用） */
#define CELL_CHAR_H 12u     /* 格子内 6x12 字体高 */

/* ---------------- 软键盘面板（QWERTY 排布，字母小写、跟随大小写状态）
 * 0 = 空格子（仅凑位，光标移动时自动跳过） ---------------- */
static const uint8_t s_keys[PANEL_ROWS][PANEL_COLS] = {
    {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 0},
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', KEY_SHIFT},
    {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', KEY_DEL, 0},
    {'z', 'x', 'c', 'v', 'b', 'n', 'm', KEY_SP, KEY_TXSW, KEY_RXSW, 0}
};

/* ---------------- 长按关机 ---------------- */
#define PWR_HOLD_MS       3000u                 /* 按住 3 秒关机 */
#define PWR_KEY_MASK      (1ul << 13u)          /* '<' 键：键位表 s_keyMap[3][1] -> 键索引 13 */

static uint8_t s_curRow = 0u;               /* 光标位置 */
static uint8_t s_curCol = 0u;
static char    s_tx[TX_MAX + 1u];           /* 发送框内容 */
static uint8_t s_txLen = 0u;
static uint8_t s_txHex = 0u;                /* 0 = 文本发送，1 = HEX 发送 */
static uint8_t s_shift = 0u;                /* 0 = 小写，1 = 大写 */
static uint32_t s_pwTick = 0u;              /* '<' 长按计时 */

/* ---------------- 绘制 ---------------- */

static void Panel_RedrawAll(void);      /* Panel_Confirm 里会调用，先声明 */

static const char *Panel_KeyText(uint8_t k, char *buf)
{
    switch (k)
    {
    case KEY_SP:    return "SP";
    case KEY_DEL:   return "<-";
    case KEY_SHIFT: return (s_shift != 0u) ? "ABC" : "abc";
    case KEY_TXSW:  return (s_txHex != 0u) ? "HEX" : "TXT";
    case KEY_RXSW:  return (SerialTerm_IsHexMode() != 0u) ? "HEX" : "TXT";
    default:
        if (k >= 'a' && k <= 'z' && s_shift != 0u)      /* 字母跟随大小写状态 */
        {
            k = (uint8_t)(k - 'a' + 'A');
        }
        buf[0] = (char)k;
        buf[1] = '\0';
        return buf;
    }
}

/* 画一个格子；光标所在格反色显示；空格子只铺白底 */
static void Panel_DrawCell(uint8_t r, uint8_t c)
{
    char     buf[2];
    const char *txt = Panel_KeyText(s_keys[r][c], buf);
    int16_t  x0 = (int16_t)(c * CELL_W);
    int16_t  y0 = (int16_t)(PANEL_Y0 + r * CELL_H);
    uint8_t  hl = (uint8_t)((r == s_curRow && c == s_curCol) ? 1u : 0u);
    uint16_t w;

    GUI_SetAsciiFont(&Disp_FontFlash_Ascii6x12);
    GUI_FillRect(x0, y0, (int16_t)(x0 + CELL_W - 1u), (int16_t)(y0 + CELL_H - 1u),
                 (hl != 0u) ? GUI_BLACK : GUI_WHITE);
    if (s_keys[r][c] == 0u) return;             /* 空格子：不画字 */

    w = GUI_TextWidth(txt, 1u);
    GUI_DrawString((int16_t)(x0 + (CELL_W - w) / 2u), (int16_t)(y0 + (CELL_H - CELL_CHAR_H) / 2u),
                   txt, (hl != 0u) ? GUI_WHITE : GUI_BLACK,
                   (hl != 0u) ? GUI_BLACK : GUI_WHITE, 1u);
}

/* 重画发送框（两行）：第一行 "> " 开头，内容超一行自动换到第二行，_ 为光标 */
static void TxBox_Draw(void)
{
    char    line[TX_COLS + 1u];
    uint8_t n1 = (s_txLen < TX_COLS) ? s_txLen : TX_COLS;
    uint8_t i;

    GUI_SetAsciiFont(&Disp_FontFlash_Ascii8x16);
    GUI_FillRect(0, (int16_t)TXBOX_Y, (int16_t)(GUI_Width() - 1u),
                 (int16_t)(TXBOX_Y + TXBOX_ROWS * 16u - 1u), GUI_WHITE);

    /* 第一行："> " + 内容前 TX_COLS 个字符 */
    GUI_DrawString(2, (int16_t)TXBOX_Y, ">", GUI_BLACK, GUI_WHITE, 1u);
    for (i = 0u; i < n1; i++) line[i] = s_tx[i];
    line[n1] = '\0';
    GUI_DrawString(12, (int16_t)TXBOX_Y, line, GUI_BLUE, GUI_WHITE, 1u);

    /* 第二行：内容的剩余部分 */
    if (s_txLen > TX_COLS)
    {
        GUI_DrawString(2, (int16_t)(TXBOX_Y + 16u), s_tx + TX_COLS, GUI_BLUE, GUI_WHITE, 1u);
    }

    /* 光标跟在内容末尾 */
    if (s_txLen <= TX_COLS)
    {
        GUI_DrawString((int16_t)(12u + s_txLen * CHAR_W), (int16_t)TXBOX_Y,
                       "_", GUI_BLACK, GUI_WHITE, 1u);
    }
    else
    {
        GUI_DrawString((int16_t)(2u + (s_txLen - TX_COLS) * CHAR_W),
                       (int16_t)(TXBOX_Y + 16u), "_", GUI_BLACK, GUI_WHITE, 1u);
    }
}

static void TxBox_Append(char ch)
{
    if (s_txLen >= TX_MAX) return;
    s_tx[s_txLen++] = ch;
    s_tx[s_txLen]   = '\0';
    TxBox_Draw();
}

static void TxBox_Backspace(void)
{
    if (s_txLen == 0u) return;
    s_tx[--s_txLen] = '\0';
    TxBox_Draw();
}

/* ---------------- 发送 ---------------- */

/* 十六进制字符 -> 0~15，非法返回 0xFF */
static uint8_t Panel_HexVal(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10u);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10u);
    return 0xFFu;
}

/* '=' 键：发送框内容经 USART3 发出，发送后清空 */
static void Panel_Send(void)
{
    uint8_t buf[TX_MAX];            /* 文本模式最多 64 字节；HEX 模式最多 32 字节 */
    uint8_t n = 0u;
    uint8_t i;

    if (s_txLen == 0u) return;

    if (s_txHex != 0u)      /* HEX：两位一组解析成字节（奇数长度时末位忽略） */
    {
        for (i = 0u; i + 1u < s_txLen; i += 2u)
        {
            uint8_t hi = Panel_HexVal(s_tx[i]);
            uint8_t lo = Panel_HexVal(s_tx[i + 1u]);

            if (hi != 0xFFu && lo != 0xFFu)
            {
                buf[n++] = (uint8_t)(hi * 16u + lo);
            }
        }
    }
    else                    /* 文本：原样发送 */
    {
        memcpy(buf, s_tx, s_txLen);
        n = s_txLen;
    }

    if (n > 0u)
    {
        HAL_UART_Transmit(&huart3, buf, n, 100u);
    }

    s_txLen = 0u;
    s_tx[0] = '\0';
    TxBox_Draw();
}

/* '5' 键：按光标所在格执行 */
static void Panel_Confirm(void)
{
    uint8_t k = s_keys[s_curRow][s_curCol];

    if (k == KEY_TXSW)                       /* 切换发送模式 */
    {
        s_txHex ^= 1u;
        Panel_DrawCell(s_curRow, s_curCol);
        TxBox_Draw();
    }
    else if (k == KEY_RXSW)                  /* 切换接收显示模式 */
    {
        SerialTerm_SetHexMode((SerialTerm_IsHexMode() == 0u) ? 1u : 0u);
        Panel_DrawCell(s_curRow, s_curCol);
    }
    else if (k == KEY_SHIFT)                 /* 大小写切换：字母全变，整块重画 */
    {
        s_shift ^= 1u;
        Panel_RedrawAll();
    }
    else if (k == KEY_DEL)
    {
        TxBox_Backspace();
    }
    else if (k == KEY_SP)
    {
        TxBox_Append(' ');
    }
    else
    {
        char ch = (char)k;

        if (ch >= 'a' && ch <= 'z' && s_shift != 0u)    /* 字母跟随大小写状态 */
        {
            ch = (char)(ch - 'a' + 'A');
        }
        TxBox_Append(ch);
    }
}

/* ---------------- 对外接口 ---------------- */

/* 重画整个面板（大小写切换后字母全变，需要整块重画） */
static void Panel_RedrawAll(void)
{
    uint8_t r, c;

    for (r = 0u; r < PANEL_ROWS; r++)
    {
        for (c = 0u; c < PANEL_COLS; c++)
        {
            Panel_DrawCell(r, c);
        }
    }
}

void SendPanel_Init(void)
{
    /* 两块区域的分界线：接收终端 | 发送框 | 软键盘 */
    GUI_DrawHLine(0, (int16_t)TXBOX_Y, (int16_t)GUI_Width(), GUI_GRAY);
    GUI_DrawHLine(0, (int16_t)PANEL_Y0, (int16_t)GUI_Width(), GUI_GRAY);

    TxBox_Draw();
    Panel_RedrawAll();
}

/* 光标移动：沿方向循环前进，自动跳过空格子（防死循环有 guard） */
static void Panel_Move(uint8_t dr, uint8_t dc)
{
    uint8_t guard;
    uint8_t r = s_curRow;
    uint8_t c = s_curCol;

    for (guard = 0u; guard < PANEL_ROWS * PANEL_COLS; guard++)
    {
        r = (uint8_t)((r + PANEL_ROWS + dr) % PANEL_ROWS);
        c = (uint8_t)((c + PANEL_COLS + dc) % PANEL_COLS);
        if (s_keys[r][c] != 0u) break;          /* 落到有效格子为止 */
    }
    s_curRow = r;
    s_curCol = c;
}

/* 返回 1 = 长按 '<' 3 秒，请求关机（由 main 执行） */
uint8_t SendPanel_Task(void)
{
    uint32_t now    = HAL_GetTick();
    uint8_t  oldRow = s_curRow;
    uint8_t  oldCol = s_curCol;
    char     key    = TM1668_GetKeyChar();

    /* 长按 '<' 3 秒 -> 关机（独立于软键盘事件，20ms 轮询计时） */
    if ((TM1668_ReadKeyMask() & PWR_KEY_MASK) != 0u)
    {
        if (s_pwTick == 0u)
        {
            s_pwTick = now;
        }
        else if ((now - s_pwTick) >= PWR_HOLD_MS)
        {
            s_pwTick = 0u;
            return 1u;
        }
    }
    else
    {
        s_pwTick = 0u;
    }

    switch (key)
    {
    case '2':                                       /* 上 */
        Panel_Move(PANEL_ROWS - 1u, 0u);
        break;
    case '8':                                       /* 下 */
        Panel_Move(1u, 0u);
        break;
    case '4':                                       /* 左 */
        Panel_Move(0u, PANEL_COLS - 1u);
        break;
    case '6':                                       /* 右 */
        Panel_Move(0u, 1u);
        break;
    case '5':                                       /* 确认 */
        Panel_Confirm();
        return 0u;
    case '=':                                       /* 发送 */
        Panel_Send();
        return 0u;
    case '.':                                       /* 大小写切换 */
        s_shift ^= 1u;
        Panel_RedrawAll();
        return 0u;
    default:
        return 0u;                                  /* 其它键忽略 */
    }

    if (s_curRow != oldRow || s_curCol != oldCol)   /* 只重画新旧两格 */
    {
        Panel_DrawCell(oldRow, oldCol);
        Panel_DrawCell(s_curRow, s_curCol);
    }

    return 0u;
}
