/**
  ******************************************************************************
  * @file    serial_term.c
  * @brief   串口终端实现：USART3 中断接收 -> 环形缓冲 -> 屏底滚动显示
  *
  *  接收用 USART3（PC10/PC11，9600）。终端区从 y=16 起（第 1 行状态栏之下），
  *  共 TERM_ROWS 行 x 16px，用字库 8x16 ASCII + 15x16 GB2312 汉字（等高混排）：
  *  每 ASCII 字符 8px 步进，汉字 2 字节 15px；每行高 16px
  ******************************************************************************
  */

#include "serial_term.h"
#include "usart.h"
#include "gui.h"
#include "disp_font_flash.h"      /* 字库 8x16 ASCII + 15x16 汉字 */
#include <string.h>

/* ---------------- 终端区布局（TERM_Y0/ROWS/COLS 见 serial_term.h） ---------------- */
#define TERM_X0      0
#define TERM_ROW_H   16u

/* ---------------- 接收环形缓冲 ---------------- */
#define TERM_RB_SIZE 256u          /* 2 的幂，取模编译成位与 */

static uint8_t          s_rxByte;  /* 中断接收单字节（HAL 拷贝目标） */
static uint8_t          s_rb[TERM_RB_SIZE];
static volatile uint16_t s_rbW = 0u;
static volatile uint16_t s_rbR = 0u;

/* ---------------- 行显示状态 ---------------- */
static char    s_lines[TERM_ROWS][TERM_COLS + 1u];  /* 已完成的行 */
static uint8_t s_rowCnt  = 0u;                      /* 已占用行数 */
static char    s_cur[TERM_COLS + 1u];               /* 正在接收的当前行 */
static uint8_t s_curLen  = 0u;
static uint16_t s_curPx  = 0u;                      /* 当前行已占像素宽（ASCII 9px，汉字 15px） */
static uint8_t s_dirty   = 0u;                      /* 有新内容待重画 */

/* ---------------- UART 接收回调 ---------------- */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        uint16_t next = (uint16_t)((s_rbW + 1u) % TERM_RB_SIZE);

        if (next != s_rbR)              /* 缓冲满：丢弃最新字节 */
        {
            s_rb[s_rbW] = s_rxByte;
            s_rbW = next;
        }
        HAL_UART_Receive_IT(&huart3, &s_rxByte, 1);   /* 重挂，继续接收 */
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        HAL_UART_Receive_IT(&huart3, &s_rxByte, 1);   /* 溢出等错误后重新挂接收 */
    }
}

/* ---------------- 行管理 ---------------- */

static uint8_t s_hexMode = 0u;      /* 0 = 文本(ASCII)，1 = HEX 显示 */
static uint8_t s_pending = 0u;      /* 待配对的 GB2312 前导字节（0 = 无） */

static void Term_FlushLine(void);   /* Term_Append 里会调用，先声明 */

/* 追加一个字符到当前行（像素宽超出行宽自动换行） */
static void Term_Append(char c)
{
    if ((s_curPx + TERM_ASCII_W > TERM_W_PX) || (s_curLen >= TERM_COLS))
    {
        Term_FlushLine();
    }
    s_cur[s_curLen++] = c;
    s_cur[s_curLen]   = '\0';
    s_curPx += TERM_ASCII_W;
    s_dirty = 1u;
}

/* 追加一个 GB2312 汉字（双字节）到当前行，像素宽不够先换行 */
static void Term_AppendCjk(uint8_t hi, uint8_t lo)
{
    if ((s_curPx + TERM_CJK_W > TERM_W_PX) || (s_curLen + 2u > TERM_COLS))
    {
        Term_FlushLine();
    }
    s_cur[s_curLen++] = (char)hi;
    s_cur[s_curLen++] = (char)lo;
    s_cur[s_curLen]   = '\0';
    s_curPx += TERM_CJK_W;
    s_dirty = 1u;
}

/* 当前行入列（满了就整体上移一行，实现滚动），然后开新行 */
static void Term_FlushLine(void)
{
    if (s_rowCnt < TERM_ROWS)
    {
        memcpy(s_lines[s_rowCnt++], s_cur, (size_t)s_curLen + 1u);
    }
    else
    {
        memmove(s_lines, s_lines[1], sizeof(s_lines) - sizeof(s_lines[0]));
        memcpy(s_lines[TERM_ROWS - 1u], s_cur, (size_t)s_curLen + 1u);
    }
    s_curLen = 0u;
    s_curPx  = 0u;
    s_cur[0] = '\0';
    s_dirty  = 1u;
}

/* 重画整个终端区（内容变了才调用，8 行文本，开销很小） */
static void Term_Redraw(void)
{
    uint8_t i;

    /* 终端用字库 8x16 ASCII + 15x16 GB2312 汉字（第 1 行状态栏会切成别的字体，这里显式设回） */
    GUI_SetAsciiFont(&Disp_FontFlash_Ascii8x16);
    GUI_SetCjkFont(&Disp_FontFlash_Cjk15x16);

    /* 只清接收区自己的范围（y=TERM_Y0 ~ TERM_Y0+ROWS*16），不能动下面的发送框/键盘 */
    GUI_FillRect(TERM_X0, (int16_t)TERM_Y0, (int16_t)(GUI_Width() - 1u),
                 (int16_t)(TERM_Y0 + TERM_ROWS * TERM_ROW_H - 1u), GUI_GRAY);

    /* 空闲提示：用字库 15x16 汉字显示"等待数据"（\xB5\xC8=等 \xB4\xFD=待 \xCA\xFD=数 \xBE\xDD=据），
     * 收到第一个字节后重画即消失 */
    if (s_rowCnt == 0u && s_curLen == 0u)
    {
        static const char idleTip[] = "\xB5\xC8\xB4\xFD\xCA\xFD\xBE\xDD";

        GUI_DrawString((int16_t)((GUI_Width() - GUI_TextWidth(idleTip, 1u)) / 2u),
                       (int16_t)(TERM_Y0 + (TERM_ROWS * TERM_ROW_H - 16u) / 2u),
                       idleTip, GUI_BLUE, GUI_GRAY, 1);
        return;
    }

    for (i = 0u; i < s_rowCnt; i++)
    {
        GUI_DrawString(TERM_X0, (int16_t)(TERM_Y0 + i * TERM_ROW_H),
                       s_lines[i], GUI_BLACK, GUI_GRAY, 1);
    }
    if (s_curLen > 0u)                  /* 未回车的当前行也显示出来 */
    {
        /* 行数已满时叠画在最后一行，避免越界盖到下面的发送框（回车后恢复正常） */
        uint8_t row = (s_rowCnt < TERM_ROWS) ? s_rowCnt : (uint8_t)(TERM_ROWS - 1u);

        GUI_DrawString(TERM_X0, (int16_t)(TERM_Y0 + row * TERM_ROW_H),
                       s_cur, GUI_BLACK, GUI_GRAY, 1);
    }
}

/* ---------------- 对外接口 ---------------- */

void SerialTerm_Init(void)
{
    Term_Redraw();                        /* 先铺一层灰色接收区背景 */
    HAL_UART_Receive_IT(&huart3, &s_rxByte, 1);
}

uint8_t SerialTerm_Task(void)
{
    static const char hexTab[] = "0123456789ABCDEF";
    uint8_t shutdownReq = 0u;
    uint8_t ch;

    while (s_rbR != s_rbW)
    {
        ch    = s_rb[s_rbR];
        s_rbR = (uint16_t)((s_rbR + 1u) % TERM_RB_SIZE);

//        if (ch == 'o' || ch == 'O')                 /* 验收命令：关机 */
//        {
//            shutdownReq = 1u;
//            continue;
//        }

        if (s_hexMode != 0u)                        /* HEX：每字节两位十六进制 + 空格 */
        {
            s_pending = 0u;                         /* 模式内不成对，丢弃半包前导 */
            if (ch == '\r' || ch == '\n') continue;
            Term_Append(hexTab[ch >> 4]);
            Term_Append(hexTab[ch & 0x0Fu]);
            Term_Append(' ');
            continue;
        }

        if (s_pending != 0u)                        /* 上一字节是 GB2312 前导字节 */
        {
            uint8_t hi = s_pending;

            s_pending = 0u;
            if (ch >= 0xA1u && ch <= 0xFEu)         /* 后续字节合法：配成汉字入列 */
            {
                Term_AppendCjk(hi, ch);
            }
            /* 后续字节非法：前导已丢弃，本字节下轮按普通字符处理 */
            continue;
        }

        if (ch >= 0xA1u && ch <= 0xF7u)             /* GB2312 前导字节：等下一字节配对 */
        {
            s_pending = ch;
            continue;
        }

        if (ch == '\r') continue;
        if (ch == '\n')                             /* 换行 */
        {
            Term_FlushLine();
            continue;
        }
        if (ch < 0x20u || ch > 0x7Eu) continue;     /* 只显示可打印 ASCII */
        Term_Append((char)ch);
    }

    if (s_dirty != 0u)
    {
        s_dirty = 0u;
        Term_Redraw();
    }

    return shutdownReq;
}

void SerialTerm_SetHexMode(uint8_t hex)
{
    if (s_hexMode == (hex ? 1u : 0u)) return;

    s_hexMode = hex ? 1u : 0u;

    /* 模式切换：清空显示重新开始 */
    s_rowCnt  = 0u;
    s_curLen  = 0u;
    s_curPx   = 0u;
    s_cur[0]  = '\0';
    s_pending = 0u;
    s_dirty   = 1u;
}

uint8_t SerialTerm_IsHexMode(void)
{
    return s_hexMode;
}
