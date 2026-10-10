/**
  ******************************************************************************
  * @file    power_adc.c
  * @brief   电源电压采集实现（PC0 充电/电源，PC1 电池，33K+33K 分压）
  *
  *  支持两种 ADC 配置（用 PWR_ADC_SEQ_SCAN 切换）：
  *
  *  ① 扫描双通道（PWR_ADC_SEQ_SCAN = 1，与当前 CubeMX 配置对应）
  *       CubeMX: ScanConvMode = Enable, NbrOfConversion = 2,
  *               Rank1 = ADC_CHANNEL_10(PC0), Rank2 = ADC_CHANNEL_11(PC1)
  *       读法：Start 一次 → 连读两个 EOC，分别就是 PC0 / PC1 的值
  *       （ContinuousConvMode 开或关都能用；建议关掉更省电，也不会出现读错通道）
  *
  *  ② 单通道轮询（PWR_ADC_SEQ_SCAN = 0）
  *       CubeMX: ScanConvMode = Disable, NbrOfConversion = 1
  *       读法：每次读之前用 HAL_ADC_ConfigChannel() 切换通道
  ******************************************************************************
  */

#include "power_adc.h"

/* 与当前 CubeMX 配置一致：扫描双通道。若你改回单通道，把它改成 0 */
#ifndef PWR_ADC_SEQ_SCAN
#define PWR_ADC_SEQ_SCAN      1
#endif

/* 扫描模式下 rank1 的采样时间，需与 CubeMX 里设置保持一致（改回时要同步） */
#define PWR_ADC_RANK1_SAMPLETIME   ADC_SAMPLETIME_71CYCLES_5
/* 内部参考(VREFINT)要求较长采样时间，手册建议 >= 5.1us */
#define PWR_ADC_VREF_SAMPLETIME    ADC_SAMPLETIME_239CYCLES_5

/* 分压点源阻抗 33K||33K = 16.5K，需要的采样时间约 1.2us，
 * 71.5 周期 @12MHz ADC 时钟 ≈ 6us，余量充足；单通道模式用 239.5 周期更保险 */

static uint8_t  s_ready     = 0u;
static uint8_t  s_pairValid = 0u;
static uint32_t s_pairTick  = 0u;
static uint16_t s_rawPower  = 0u;   /* PC0 */
static uint16_t s_rawBat    = 0u;   /* PC1 */
static uint8_t  s_lastErr   = 0u;   /* 最近一次错误码，见 power_adc.h */

/* ---------------- 内部函数 ---------------- */

static uint16_t PowerAdc_SampleOnce(void)
{
    uint16_t value = 0u;

    if (HAL_ADC_Start(&hadc1) != HAL_OK) return 0u;
    if (HAL_ADC_PollForConversion(&hadc1, 10u) == HAL_OK)
    {
        value = (uint16_t)(HAL_ADC_GetValue(&hadc1) & 0x0FFFu);
    }
    HAL_ADC_Stop(&hadc1);

    return value;
}

/* 单通道采样 N 次，去掉最大最小后取平均 */
static uint8_t PowerAdc_AverageSingle(uint16_t *out)
{
    uint32_t sum = 0u;
    uint16_t v, vmin = 0xFFFFu, vmax = 0u;
    uint8_t  i;

    if (out == NULL) return 1u;

    for (i = 0u; i < PWR_ADC_AVG_SAMPLES; i++)
    {
        v = PowerAdc_SampleOnce();
        sum += v;
        if (v < vmin) vmin = v;
        if (v > vmax) vmax = v;
    }

#if (PWR_ADC_AVG_SAMPLES > 2u)
    sum -= vmin;
    sum -= vmax;
    *out = (uint16_t)(sum / (PWR_ADC_AVG_SAMPLES - 2u));
#else
    *out = (uint16_t)(sum / PWR_ADC_AVG_SAMPLES);
#endif
    return 0u;
}

#if (PWR_ADC_SEQ_SCAN == 1)

static uint8_t PowerAdc_ConfigRank1(uint32_t channel, uint32_t samplingTime)
{
    ADC_ChannelConfTypeDef cfg;

    cfg.Channel      = channel;
    cfg.Rank         = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = samplingTime;

    return (HAL_ADC_ConfigChannel(&hadc1, &cfg) == HAL_OK) ? 0u : 1u;
}

/* 启动一次转换，按顺序读出 rank1 / rank2（= PC0 / PC1） */
static uint8_t PowerAdc_ReadPairOnce(uint16_t *v1, uint16_t *v2)
{
    /* STM32F1 上 EOC 是"读 DR 清零"，所以这里读一次 DR 把可能的残留标志清掉 */
    (void)HAL_ADC_GetValue(&hadc1);

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        s_lastErr = 1u;
        return 1u;
    }

    if (HAL_ADC_PollForConversion(&hadc1, 10u) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        s_lastErr = 2u;
        return 2u;
    }
    *v1 = (uint16_t)(HAL_ADC_GetValue(&hadc1) & 0x0FFFu);      /* rank1 = PC0 */

    if (HAL_ADC_PollForConversion(&hadc1, 10u) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        s_lastErr = 3u;
        return 3u;
    }
    *v2 = (uint16_t)(HAL_ADC_GetValue(&hadc1) & 0x0FFFu);      /* rank2 = PC1 */

    HAL_ADC_Stop(&hadc1);
    s_lastErr = 0u;
    return 0u;
}

/* 采 N 组，每组两个通道，分别去极值求平均 */
static uint8_t PowerAdc_UpdatePair(void)
{
    uint32_t sum1 = 0u, sum2 = 0u;
    uint16_t a, b;
    uint16_t min1 = 0xFFFFu, max1 = 0u, min2 = 0xFFFFu, max2 = 0u;
    uint8_t  i, ret;

    for (i = 0u; i < PWR_ADC_AVG_SAMPLES; i++)
    {
        ret = PowerAdc_ReadPairOnce(&a, &b);
        if (ret != 0u) return ret;

        sum1 += a; sum2 += b;
        if (a < min1) min1 = a;
        if (a > max1) max1 = a;
        if (b < min2) min2 = b;
        if (b > max2) max2 = b;
    }

#if (PWR_ADC_AVG_SAMPLES > 2u)
    sum1 -= min1; sum1 -= max1;
    sum2 -= min2; sum2 -= max2;
    s_rawPower = (uint16_t)(sum1 / (PWR_ADC_AVG_SAMPLES - 2u));
    s_rawBat   = (uint16_t)(sum2 / (PWR_ADC_AVG_SAMPLES - 2u));
#else
    s_rawPower = (uint16_t)(sum1 / PWR_ADC_AVG_SAMPLES);
    s_rawBat   = (uint16_t)(sum2 / PWR_ADC_AVG_SAMPLES);
#endif

    return 0u;
}

/* 100ms 内复用同一组采样结果，避免上层一次刷新里重复启动转换 */
static void PowerAdc_EnsurePair(void)
{
    uint8_t ret;

    if ((s_pairValid != 0u) && ((uint32_t)(HAL_GetTick() - s_pairTick) < 100u)) return;

    ret = PowerAdc_UpdatePair();
    if (ret == 0u)
    {
        s_pairValid = 1u;
        s_pairTick  = HAL_GetTick();
    }
    else
    {
        s_lastErr = (uint8_t)(0x10u | ret);     /* 多组平均过程中出错 */
    }
}

#else   /* ---------------- 单通道轮询模式 ---------------- */

static uint8_t PowerAdc_SetChannel(uint32_t channel)
{
    ADC_ChannelConfTypeDef cfg;

    cfg.Channel      = channel;
    cfg.Rank         = ADC_REGULAR_RANK_1;
    cfg.SamplingTime = PWR_ADC_VREF_SAMPLETIME;

    return (HAL_ADC_ConfigChannel(&hadc1, &cfg) == HAL_OK) ? 0u : 1u;
}

#endif

/* ---------------- 对外接口 ---------------- */

/**
  * @brief  ADC 自校准（可随时调用：复位后、电源稳定后各来一次最稳）
  * @retval 0 = 成功
  */
uint8_t PowerAdc_Calibrate(void)
{
    (void)HAL_ADC_Stop(&hadc1);          /* 未启动时返回错误，忽略即可 */

    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
    {
        s_lastErr = 0x20u;
        return 1u;
    }

    s_pairValid = 0u;                    /* 校准后缓存作废 */
    s_lastErr   = 0u;
    return 0u;
}

uint8_t PowerAdc_LastError(void)
{
    return s_lastErr;
}

uint8_t PowerAdc_Init(void)
{
    uint16_t dummy1, dummy2;

    if (PowerAdc_Calibrate() != 0u) return 1u;

    s_ready = 1u;

#if (PWR_ADC_SEQ_SCAN == 1)
    /* 先空跑一组丢掉：切换后的首次转换数据不可靠 */
    (void)PowerAdc_UpdatePair();
    (void)dummy1; (void)dummy2;
    s_pairValid = 0u;
#else
    if (PowerAdc_SetChannel(ADC_CHANNEL_10) != 0u) return 2u;
    (void)PowerAdc_SampleOnce();
    (void)PowerAdc_AverageSingle(&dummy1);      /* PC0 建立一次 */
    if (PowerAdc_SetChannel(ADC_CHANNEL_11) != 0u) return 3u;
    (void)PowerAdc_SampleOnce();
    (void)PowerAdc_AverageSingle(&dummy2);      /* PC1 建立一次 */
#endif

    return 0u;
}

uint8_t PowerAdc_IsReady(void)
{
    return s_ready;
}

uint16_t PowerAdc_ReadRaw(uint8_t ch)
{
    if (s_ready == 0u) return 0u;

#if (PWR_ADC_SEQ_SCAN == 1)
    PowerAdc_EnsurePair();
    return (ch == PWR_ADC_CH_POWER) ? s_rawPower : s_rawBat;
#else
    {
        uint16_t raw = 0u;
        uint32_t channel = (ch == PWR_ADC_CH_POWER) ? ADC_CHANNEL_10 : ADC_CHANNEL_11;

        if (PowerAdc_SetChannel(channel) != 0u) return 0u;
        if (PowerAdc_AverageSingle(&raw) != 0u) return 0u;
        return raw;
    }
#endif
}

uint32_t PowerAdc_RawToMv(uint16_t raw)
{
    uint32_t vadc_mv;

    if (raw > 4095u) raw = 4095u;

    /* 分压点电压 = raw / 4095 * VREF */
    vadc_mv = (uint32_t)raw * PWR_ADC_VREF_MV / 4095u;

    /* 折算回测量点：Vx = Vadc * DEN / NUM （两个 33K -> 乘 2） */
    return vadc_mv * PWR_ADC_DIV_DEN / PWR_ADC_DIV_NUM;
}

uint32_t PowerAdc_ReadMv(uint8_t ch)
{
    uint16_t raw = PowerAdc_ReadRaw(ch);

#if (PWR_ADC_USE_VREFINT == 1)
    {
        uint32_t vdda = PowerAdc_ReadVddaMv();
        uint32_t vadc_mv;

        if (vdda < 1000u || vdda > 4000u) vdda = PWR_ADC_VREF_MV;   /* 估算异常就退回默认值 */
        vadc_mv = (uint32_t)raw * vdda / 4095u;
        return vadc_mv * PWR_ADC_DIV_DEN / PWR_ADC_DIV_NUM;
    }
#else
    return PowerAdc_RawToMv(raw);
#endif
}

uint32_t PowerAdc_PowerMv(void)
{
    return PowerAdc_ReadMv(PWR_ADC_CH_POWER);
}

uint32_t PowerAdc_BatMv(void)
{
    return PowerAdc_ReadMv(PWR_ADC_CH_BAT);
}

uint32_t PowerAdc_ReadVddaMv(void)
{
    uint16_t raw = 0u;

#if (PWR_ADC_SEQ_SCAN == 1)
    /* 临时把 rank1 换成内部参考通道，读完恢复成 PC0；采样结束再恢复可避免破坏缓存 */
    if (PowerAdc_ConfigRank1(ADC_CHANNEL_VREFINT, PWR_ADC_VREF_SAMPLETIME) != 0u) return 0u;
    if (PowerAdc_AverageSingle(&raw) != 0u) return 0u;
    (void)PowerAdc_ConfigRank1(ADC_CHANNEL_10, PWR_ADC_RANK1_SAMPLETIME);
    s_pairValid = 0u;                       /* rank1 动过了，让缓存失效 */
#else
    if (PowerAdc_SetChannel(ADC_CHANNEL_VREFINT) != 0u) return 0u;
    if (PowerAdc_AverageSingle(&raw) != 0u) return 0u;
#endif

    if (raw == 0u) return 0u;

    /* VREFINT 典型 1.20V：VDDA = 1200mV * 4095 / raw */
    return (uint32_t)1200u * 4095u / raw;
}
