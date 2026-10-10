/**
  ******************************************************************************
  * @file    power_ctrl.c
  * @brief   按键开机 / 软件自锁供电 / 长按关机
  *
  *  长按关机的三个必要条件：
  *    1) 按键必须被稳定读到高电平（PE1 悬空会误判 → 打开内部下拉）；
  *    2) 开机后必须见过一次“松开”，否则一开机手还按着就被判成长按；
  *    3) PE0(POW_SOFT_EN) 拉低且一直保持低电平。
  ******************************************************************************
  */

#include "power_ctrl.h"

#define POWER_KEY_DEBOUNCE_MS    30u      /* 按键去抖时间 */
#define POWER_ARM_FALLBACK_MS    10000u   /* 兜底：开机 10s 后仍未见到“松开”也解锁 */

static uint8_t  s_pressed    = 0;   /* 去抖后的稳定状态 */
static uint8_t  s_armed      = 0;   /* 见过一次“松开”后才允许长按关机 */
static uint8_t  s_rawPrev    = 0;   /* 上一次原始电平 */
static uint32_t s_rawChange  = 0;   /* 原始电平最后一次变化的时刻 */
static uint32_t s_pressStart = 0;   /* 本次按下开始时刻 */
static uint32_t s_bootMs     = 0;
static uint8_t  s_inited     = 0;

/**
  * @brief  上电第一时间把 POW_SOFT_EN(PE0) 拉高，接管按键维持供电
  *         直接操作寄存器：此时 HAL/SystemClock 都还没初始化
  */
void POWER_SelfHoldEarly(void)
{
    uint32_t tmp;

    __HAL_RCC_GPIOE_CLK_ENABLE();           /* 开 GPIOE 时钟 */

    tmp  = GPIOE->CRL;
    tmp &= ~(0x0Fu << 0);                   /* 清 CNF0[1:0] / MODE0[1:0] */
    tmp |=  (0x03u << 0);                   /* MODE0=11(50MHz) CNF0=00(通用推挽) */
    GPIOE->CRL = tmp;

    GPIOE->BSRR = (uint32_t)GPIO_PIN_0;     /* PE0 = 1，自锁供电 */
}

void POWER_SelfHold(void)
{
    HAL_GPIO_WritePin(POW_SOFT_EN_GPIO_Port, POW_SOFT_EN_Pin, GPIO_PIN_SET);
}

/**
  * @brief  配置按键引脚（CubeMX 生成的是无上下拉，这里按需要补内部下拉）
  */
void POWER_Init(void)
{
    GPIO_InitTypeDef gi = {0};

    gi.Pin   = POWER_KEY_TEST_Pin;
    gi.Mode  = GPIO_MODE_INPUT;
#if (POWER_KEY_USE_PULLDOWN == 1)
    gi.Pull  = GPIO_PULLDOWN;
#else
    gi.Pull  = GPIO_NOPULL;
#endif
    gi.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(POWER_KEY_TEST_GPIO_Port, &gi);

    s_bootMs    = HAL_GetTick();
    s_rawPrev   = POWER_KeyRaw();
    s_rawChange = s_bootMs;
    s_pressed   = s_rawPrev;
    if (s_pressed) s_pressStart = s_bootMs;
    s_inited    = 1;
}

uint8_t POWER_KeyRaw(void)
{
    if (HAL_GPIO_ReadPin(POWER_KEY_TEST_GPIO_Port, POWER_KEY_TEST_Pin) == GPIO_PIN_SET)
        return 1u;
    return 0u;
}

/**
  * @brief  按键状态机：去抖 + 解锁 + 按下计时
  */
void POWER_Poll(uint32_t now)
{
    uint8_t raw;

    if (!s_inited) POWER_Init();

    raw = POWER_KeyRaw();

    if (raw != s_rawPrev)               /* 电平抖动中，重新计时 */
    {
        s_rawPrev   = raw;
        s_rawChange = now;
        return;
    }
    if ((now - s_rawChange) < POWER_KEY_DEBOUNCE_MS) return;
    if (raw == s_pressed) return;       /* 稳定值没变 */

    s_pressed = raw;                    /* 稳定翻转 */
    if (s_pressed)
    {
        s_pressStart = now;             /* 按下：开始计时 */
    }
    else
    {
        s_armed = 1;                    /* 松开：解锁长按关机 */
    }

    /* 兜底：按键一直被判为高（例如悬空）时，开机 10s 后也解锁，
     * 但把计时起点重置为当前，要求重新按满设定时长 */
    if (!s_armed && ((now - s_bootMs) >= POWER_ARM_FALLBACK_MS))
    {
        s_armed = 1;
        if (s_pressed) s_pressStart = now;
    }
}

uint8_t POWER_KeyIsPressed(void)
{
    return s_pressed;
}

uint32_t POWER_PressDuration(void)
{
    if (!s_pressed) return 0u;
    return (uint32_t)(HAL_GetTick() - s_pressStart);
}

uint8_t POWER_LongPress(uint32_t holdMs)
{
    if (!s_armed)   return 0u;
    if (!s_pressed) return 0u;
    if ((uint32_t)(HAL_GetTick() - s_pressStart) >= holdMs) return 1u;
    return 0u;
}

void POWER_Shutdown(void)
{
    /* 一直保持低电平：如果按键本身还压着供电，等用户松手后系统即断电 */
    for (;;)
    {
        HAL_GPIO_WritePin(POW_SOFT_EN_GPIO_Port, POW_SOFT_EN_Pin, GPIO_PIN_RESET);
        __NOP();
    }
}
