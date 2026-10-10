/**
  ******************************************************************************
  * @file    tm1668.c
  * @brief   TM1668 按键扫描实现（软件三线时序）
  *
  *  协议要点（以 TM1668 手册为准）：
  *    - STB 拉低开始一帧，拉高结束；整帧期间 STB 保持低
  *    - 数据在 CLK 的上升沿被芯片锁存（写）；读数据时按位产生时钟并采样
  *    - 字节均 LSB 在前
  *    - DIO 为开漏 + 上拉：读之前先把 DIO 释放（写 1）
  ******************************************************************************
  */

#include "tm1668.h"   /* 内部已包含 tm1668_cfg.h（全部硬件配置：引脚/时序/采样沿） */

/* ---------------- 底层延时（参数在 tm1668_cfg.h） ---------------- */

static void TM_DelayBit(void)
{
    volatile uint32_t i;
    for (i = 0u; i < TM1668_BIT_DELAY_LOOPS; i++) { __NOP(); }
}

static void TM_DelayFrame(void)
{
    volatile uint32_t i;
    for (i = 0u; i < TM1668_FRAME_DELAY_LOOPS; i++) { __NOP(); }
}

/* ---------------- 底层时序 ---------------- */

/* 写一个字节（LSB 在前，上升沿锁存） */
static void TM_WriteByte(uint8_t data)
{
    uint8_t i;

    for (i = 0u; i < 8u; i++)
    {
        TM_CLK_L();
        if ((data & (1u << i)) != 0u) TM_DIO_H();
        else                          TM_DIO_L();
        TM_DelayBit();
        TM_CLK_H();                  /* 上升沿：芯片锁存 */
        TM_DelayBit();
    }
    /* 字节间 DIO 释放，避免总线冲突 */
    TM_DIO_H();
}

/* 读一个字节（LSB 在前），调用前必须已释放 DIO
 * 与参考例程 Incept_data() 完全一致的时序（关键！）：
 *   - 发完 0x42 命令后，第 1 位数据(bit0)已经出现在 DIO 上，
 *     进读循环【不能】先打下降沿，否则 bit0 被扔掉、整字节错位！
 *   - 循环：采样(此时 CLK 为高) -> CLK 拉低(下降沿，芯片移出下一位) -> CLK 拉高 */
static uint8_t TM_ReadByte(void)
{
    uint8_t i, data = 0u;
//	TM_CLK_L();
//    TM_DIO_H();                      /* 开漏释放，靠上拉为高（等效参考代码把数据线设为输入） */
    TM_DelayBit();

    for (i = 0u; i < 8u; i++)
    {
        TM_CLK_L();                  /* 下降沿：芯片移出下一位 */
        TM_DelayBit();

        data >>= 1;                  /* 先读出的是低位 */
        if (TM_DIO_READ() != GPIO_PIN_RESET) data |= 0x80u;

        TM_DelayBit();
        TM_CLK_H();                  /* 确保高电平后采样（第 1 位已在 DIO 上，无需先打沿） */
    }

    TM_CLK_H();                      /* 恢复空闲态 CLK=H */
    return data;
}

/* ---------------- 初始化 ---------------- */

uint8_t TM1668_Init(void)
{
    /* 1. 显示模式设置命令（参考例程发 0x03；取值见 tm1668_cfg.h） */
    TM_STB_L();
    TM_DelayFrame();
    TM_WriteByte(TM1668_MODE_CMD);
    TM_DelayFrame();
    TM_STB_H();
    TM_DelayFrame();

    /* 2. 打开显示：显示关断时芯片不扫描 SEG/KS，键扫会一直读不到键。
     *    亮度给最低档（1/16），板上没有挂 LED 时也不会有明显影响 */
    TM1668_SetDisplay(1u, 0u);

    /* 3. 进入协议空闲态：STB=H、CLK=H、DIO=释放（开漏写 1 即释放，靠上拉为高） */
    TM_STB_H();
    TM_CLK_H();
    TM_DIO_H();
    TM_DelayFrame();

    return 0u;
}

/* ---------------- 键值读取 ---------------- */

/* 键位表：行 = 键扫数据第 n 字节，列 = 压缩后低 4 位的 bit0~3
 * 物理顺序：KS1/K1, KS1/K2, KS2/K1, KS2/K2, KS3/K1 … KS10/K2
 * 面板丝印跟这里不一致时，只改这张表即可 */
static const char s_keyMap[TM1668_KEY_DATA_BYTES][4u] = {
    {'+', '>', '-', '3'},    {'*', '6', '/', '9'},    {'E', '#', '2', '1'},    {'%', '<', '5', '4'},
    {'8', '7', '0', '.'}
};

/*static*/ uint8_t s_keyRaw[TM1668_KEY_DATA_BYTES];   /* 最近一次读到的原始 5 字节（调试可看） */

/**
  * @brief  读一次键扫原始数据
  *         时序：STB低 -> 命令0x42(读键扫) -> 释放DIO -> 读 5 字节 -> STB高
  */
uint32_t TM1668_ReadKeyMask(void)
{
    uint8_t  n, i;
    uint32_t mask = 0u;

    TM_STB_L();
    TM_DelayFrame();
    TM_WriteByte(0x42u);            /* 读键扫数据命令 */
    TM_DelayFrame();

    for (n = 0u; n < TM1668_KEY_DATA_BYTES; n++)
    {
        s_keyRaw[n] = TM_ReadByte();
    }

    TM_STB_H();                     /* 帧结束 */
    TM_DelayFrame();

    /* 手册：每字节只装 2 条 KS —— bit0/1 = KS(2n+1) 的 K1/K2，bit3/4 = KS(2n+2) 的 K1/K2，
     * bit2/5/6/7 是无效位。把 bit3/4 右移 1 位，低 4 位就凑成一行"按键矩阵"：
     * bit0~3 = 4 个键，键索引 idx = n*4 + i */
    for (n = 0u; n < TM1668_KEY_DATA_BYTES; n++)
    {
        uint8_t row = (uint8_t)((s_keyRaw[n] & 0x03u) | ((s_keyRaw[n] & 0x18u) >> 1));

        for (i = 0u; i < 4u; i++)
        {
            if ((row & (1u << i)) != 0u)
            {
                mask |= (uint32_t)(1u << (n * 4u + i));
            }
        }
    }

    return mask;
}

/* ---------------- 按下列表（按按下先后顺序） ---------------- */

static uint8_t  s_pressed    = 0u;   /* 当前是否有键按着（用于只上报一次） */
/*static*/ uint32_t s_stableMask = 0u;   /* 消抖：上次读到的按键掩码 */
static uint8_t  s_stableCnt  = 0u;   /* 消抖：连续读到相同掩码的次数 */

static uint8_t s_keyList[TM1668_KEY_LIST_MAX];   /* 按下的键索引，按按下先后排列 */
static uint8_t s_keyListCnt = 0u;

/**
  * @brief  用最新（已消抖）的掩码刷新按下列表：
  *         松开的键从列表里删除、后面的整体前移；新按下的键追加到末尾。
  */
static void KeyList_Update(uint32_t mask)
{
    uint8_t i, j;

    for (i = 0u; i < s_keyListCnt; )            /* 已松开的键：删除，后面的前移 */
    {
        if ((mask & (1u << s_keyList[i])) == 0u)
        {
            for (j = i + 1u; j < s_keyListCnt; j++) s_keyList[j - 1u] = s_keyList[j];
            s_keyListCnt--;
        }
        else
        {
            i++;
        }
    }

    for (i = 0u; i < TM1668_KEY_NUM; i++)       /* 新按下的键：追加到末尾 */
    {
        if ((mask & (1u << i)) == 0u) continue;

        for (j = 0u; j < s_keyListCnt; j++)
        {
            if (s_keyList[j] == i) break;       /* 已经在列表里就不重复加 */
        }
        if ((j >= s_keyListCnt) && (s_keyListCnt < TM1668_KEY_LIST_MAX))
        {
            s_keyList[s_keyListCnt++] = i;
        }
    }
}

/**
  * @brief  取当前按下的键（按按下先后顺序）
  * @param  out    输出缓冲区，依次写入键位表里的字符，末尾补 '\0'
  * @param  maxLen out 的容量（含 '\0'）
  * @retval 当前按下的键个数
  */
uint8_t TM1668_GetPressedKeys(char *out, uint8_t maxLen)
{
    uint8_t i;
    uint8_t n = (s_keyListCnt < maxLen) ? s_keyListCnt : maxLen;

    for (i = 0u; i < n; i++)
    {
        uint8_t idx = s_keyList[i];
        out[i] = s_keyMap[idx >> 2u][idx & 0x03u];
    }
    if (n < maxLen) out[n] = '\0';

    return n;
}

/**
  * @brief  主循环周期调用（建议 20~50ms 一次）
  *         连续 TM1668_DEBOUNCE_CNT 次读到相同按键才生效（消抖），
  *         同时刷新按下列表；同一次按压只上报一次，松开后才允许下一次上报。
  * @retval 本次新按下的键号（索引+1，1~20）；0 = 无按键事件
  */
uint8_t TM1668_GetKey(void)
{
    uint32_t mask = TM1668_ReadKeyMask();
    uint8_t  idx;

    /* 消抖：连续 TM1668_DEBOUNCE_CNT 次读到同样的掩码才认为有效 */
    if (mask == s_stableMask)
    {
        if (s_stableCnt < 0xFFu) s_stableCnt++;
    }
    else
    {
        s_stableMask = mask;
        s_stableCnt  = 0u;
    }
    if (s_stableCnt < TM1668_DEBOUNCE_CNT) return 0u;

    KeyList_Update(mask);

    if (mask == 0u)
    {
        s_pressed = 0u;                     /* 全部松开 */
        return 0u;
    }
    if (s_pressed != 0u) return 0u;         /* 还按着，等松开 */
    s_pressed = 1u;

    for (idx = 0u; idx < TM1668_KEY_NUM; idx++)
    {
        if ((mask & (1u << idx)) != 0u) return (uint8_t)(idx + 1u);
    }
    return 0u;
}

/* ---------------- 显示控制（可选功能） ---------------- */

/**
  * @brief  显示开/关与亮度。命令位布局（TM1668 手册）：
  *         0x80 | 亮度(bit0~2) | 显示开关(bit3)
  *         0x80 = 显示关，0x88~0x8F = 显示开 + 1/16~14/16 亮度
  */
void TM1668_SetDisplay(uint8_t on, uint8_t brightness)
{
    uint8_t cmd = (uint8_t)(0x80u | ((brightness & 0x07u)));

    if (on) cmd |= 0x08u;

    TM_STB_L();
    TM_DelayFrame();
    TM_WriteByte(cmd);
    TM_DelayFrame();
    TM_STB_H();
}
