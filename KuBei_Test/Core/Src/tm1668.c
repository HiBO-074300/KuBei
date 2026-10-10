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

    TM_DIO_H();                      /* 开漏释放，靠上拉为高（等效参考代码把数据线设为输入） */
    TM_DelayBit();

    for (i = 0u; i < 8u; i++)
    {
        TM_CLK_H();                  /* 确保高电平后采样（第 1 位已在 DIO 上，无需先打沿） */
        TM_DelayBit();

        data >>= 1;                  /* 先读出的是低位 */
        if (TM_DIO_READ() != GPIO_PIN_RESET) data |= 0x80u;

        TM_CLK_L();                  /* 下降沿：芯片移出下一位 */
        TM_DelayBit();
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

    /* 2. 进入协议空闲态：STB=H、CLK=H、DIO=释放（开漏写 1 即释放，靠上拉为高） */
    TM_STB_H();
    TM_CLK_H();
    TM_DIO_H();
    TM_DelayFrame();

    return 0u;
}

/* ---------------- 键值读取 ---------------- */

/**
  * @brief  读一次键扫原始数据
  *         时序：STB低 -> 命令0x42(读键扫) -> 释放DIO -> 读 5 字节 -> STB高
  */
uint16_t TM1668_ReadKeyMask(void)
{
    uint8_t  data[TM1668_KEY_DATA_BYTES];
    uint8_t  n, i;
    uint16_t mask = 0u;

    TM_STB_L();
    TM_DelayFrame();
    TM_WriteByte(0x42u);            /* 读键扫数据命令 */
    TM_DelayFrame();

    for (n = 0u; n < TM1668_KEY_DATA_BYTES; n++)
    {
        data[n] = TM_ReadByte();
    }

    TM_STB_H();                     /* 帧结束 */
    TM_DelayFrame();

    /* 展开：第 n 字节的 bit[2m]/bit[2m+1] = K1/K2 与 SEG(4n+m+1)
     * 键索引 idx = (SEG-1)*2 + (K-1) */
    for (n = 0u; n < TM1668_KEY_DATA_BYTES; n++)
    {
        for (i = 0u; i < 8u; i++)
        {
            if ((data[n] & (1u << i)) != 0u)
            {
                uint8_t seg = (uint8_t)((i >> 1) + 1u);   /* 该字节内第几个 SEG */
                uint8_t k   = (uint8_t)((i & 1u) + 1u);   /* K1 / K2 */
                uint16_t idx = (uint16_t)((n * 4u + (seg - 1u)) * 2u + (k - 1u));
                if (idx < TM1668_KEY_NUM) mask |= (uint16_t)(1u << idx);
            }
        }
    }

    return mask;
}

/* ---------------- 键值映射（不做消抖，按下立即上报） ---------------- */

static const uint8_t *s_keyMap   = NULL;
static uint8_t        s_lastKey  = 0u;
static uint8_t        s_pressed  = 0u;   /* 当前是否有键按着（用于只上报一次） */

void TM1668_SetKeyMap(const uint8_t *map20)
{
    s_keyMap = map20;
}

uint8_t TM1668_GetLastKey(void)
{
    return s_lastKey;
}

/**
  * @brief  主循环周期调用（建议 20~50ms 一次）
  *         不做消抖：读到键按下立即返回键值；同一次按压只报一次，
  *         松开后才允许下一次上报。
  * @retval 本次按下的键值；0 = 无按键事件
  */
uint8_t TM1668_GetKey(void)
{
    uint16_t mask = TM1668_ReadKeyMask();
    uint8_t  idx;

    if (mask == 0u)
    {
        s_pressed = 0u;                     /* 全部松开 */
        return 0u;
    }
    if (s_pressed != 0u) return 0u;         /* 还按着，等松开 */
    s_pressed = 1u;

    for (idx = 0u; idx < TM1668_KEY_NUM; idx++)
    {
        if ((mask & (1u << idx)) != 0u) break;
    }
    if (idx >= TM1668_KEY_NUM) return 0u;

    s_lastKey = (s_keyMap != NULL) ? s_keyMap[idx] : (uint8_t)(idx + 1u);
    return s_lastKey;
}

/* ---------------- 显示控制（可选功能） ---------------- */

/**
  * @brief  显示开/关与亮度。命令位布局以 TM1668 手册为准：
  *         0x48 | 显示开关(bit3) | 亮度(bit0~2)
  */
void TM1668_SetDisplay(uint8_t on, uint8_t brightness)
{
    uint8_t cmd = (uint8_t)(0x48u | ((brightness & 0x07u)));

    if (on) cmd |= 0x08u;

    TM_STB_L();
    TM_DelayFrame();
    TM_WriteByte(cmd);
    TM_DelayFrame();
    TM_STB_H();
}
