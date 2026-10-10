/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "gui.h"              /* 图形库（与硬件解耦，只认 GUI_DispDrv_t） */
#include "disp_ili9327.h"     /* 显示驱动层：ILI9327 + FSMC */
#include "disp_font_flash.h"  /* 字库层：SPI2 字库芯片 -> GUI_Font_t */
#include "power_ctrl.h"
#include "power_adc.h"        /* PC0/PC1 电源电压采集（33K+33K 分压） */
#include "tm1668.h"           /* 按键扫描芯片（PE2/PE3/PE4 软件三线） */
#include "rtc_pcf8563.h"      /* PCF8563 RTC（I2C1） */
#include "w25q64.h"           /* SPI1 上的数据 Flash */
#include "font_lib.h"         /* 仅用于诊断打印字库 ID */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t s_flash1_ok = 0;    /* SPI1 W25Q64 初始化结果 */
static uint8_t s_fontlib_ok = 0;   /* SPI2 字库          初始化结果 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void Debug_Printf(const char *fmt, ...);
static void Ui_UpdateClock(uint8_t force);
static void Ui_Init(void);
static void Ui_Update(void);
static void Ui_Task(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define POWER_OFF_HOLD_MS   3000u   /* 开机键长按多久关机 */

/* ===== 界面布局（320x240 横屏，只保留：时钟 / 电池充电电压 / 按键信息） =====
 * 默认内置字体 5x7：每字符占 6px（宽 5 + 1 空隙）
 *   标签 "BAT:" = 24px，数值 5 位 = 30px，单位 "mV" = 12px，按此留间距 */
#define UI_TIME_X   108u    /* HH:MM:SS 起点（12x24 字体，8 字符宽 104px，居中） */
#define UI_TIME_Y   32u
#define UI_ROW_X    40u     /* 行标签起始 x */
#define UI_VAL_X    100u    /* 行数值起始 x（5 位数字结束于 x≈130） */
#define UI_UNIT_X   146u    /* 单位 "mV" 起始 x，与数值留出约 16px 空隙 */
#define UI_BAT_Y    60u     /* 电池电压行 */
#define UI_CHG_Y    90u     /* 充电电压行 */
#define UI_KEY_Y    120u    /* 开机键状态行 */
#define UI_TMKEY_Y  160u    /* TM1668 矩阵键值行 */

static uint32_t s_lastRefresh = 0;

/**
  * @brief  通过 USART1(PA9, 115200) 输出调试信息。
  *         屏幕不亮时，可先用串口助手(115200)看初始化状态和读到的驱动 ID。
  */
static void Debug_Printf(const char *fmt, ...)
{
    char buf[128];
    int n;
    va_list ap;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0)
    {
        if (n > (int)sizeof(buf) - 1) n = (int)sizeof(buf) - 1;
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 100);
    }
}

/**
  * @brief  初始化两片外挂 Flash 并把结果打印到串口
  *         SPI1 -> W25Q64 数据 Flash（自检：擦/写/读/再擦）
  *         SPI2 -> 字库 Flash（读 ID + 试读一个汉字）
  */
static void Flash_Init_And_Report(void)
{
    static uint8_t gbuf[128];      /* 静态：省栈 */
    uint8_t  st;
    uint32_t id;

    /* ---- SPI1：W25Q64 ---- */
    s_flash1_ok = (W25Q64_Init() == 0u) ? 1u : 0u;
    id = W25Q64_GetID();
    Debug_Printf("[FLASH1] SPI1 W25Q64 JEDEC ID = 0x%06X (%s)\r\n",
                 (unsigned int)id, W25Q64_DescribeID(id));
    if (s_flash1_ok)
    {
        st = W25Q64_SelfTest();
        Debug_Printf("[FLASH1] self test %s (code=%u, lastErr=%u)\r\n",
                     (st == 0u) ? "PASS" : "FAIL", st, W25Q64_LastError());
        s_flash1_ok = (st == 0u) ? 1u : 0u;
    }

    /* ---- SPI2：字库（整形后由字库层包装成 GUI_Font_t） ---- */
    st = Disp_FontFlash_Init();
    id = Font_ReadFlashID();
    s_fontlib_ok = ((st == 0u) && (Disp_FontFlash_IsReady() != 0u)) ? 1u : 0u;
    Debug_Printf("[FLASH2] SPI2 font lib %s, JEDEC ID = 0x%06X\r\n",
                 s_fontlib_ok ? "OK" : "FAIL", (unsigned int)id);
    if (s_fontlib_ok)
    {
        /* 试读汉字"字"(GB2312 = 0xD7D6)，24x24 字模 72 字节 */
        st = Font_ReadChinese(0xD7D6u, gbuf, FONT_SIZE_24x24);
        Debug_Printf("[FLASH2] read CJK 0xD7D6 @24x24 -> %s\r\n",
                     (st == FONT_OK) ? "OK" : "FAIL");
        s_fontlib_ok = (st == FONT_OK) ? 1u : 0u;
    }
}

/**
  * @brief  刷新屏幕上的 RTC 时钟（HH:MM:SS 大字），500ms 节流、数值变化才重画
  * @param  force 1 = 强制重画（上电首次），0 = 内部节流
  */
static void Ui_UpdateClock(uint8_t force)
{
    static uint32_t  s_lastRtcTick = 0;
    static RTC_Time_t s_last = { 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu };
    const GUI_Font_t *fontSave;      /* 进入时的字体，画完必须还原（否则后面全变大字） */
    RTC_Time_t t;
    char buf[12];

    fontSave = GUI_AsciiFont();

    if (force == 0u)
    {
        if ((uint32_t)(HAL_GetTick() - s_lastRtcTick) < 500u) return;
    }
    s_lastRtcTick = HAL_GetTick();

    if (RTC_ReadTime(&t) != 0u) return;                       /* 读失败：保持原显示 */
    if (force == 0u && memcmp(&t, &s_last, sizeof(t)) == 0u) return;

    /* HH:MM:SS（字库 12x24 字体，比 16x32 小一档） */
    buf[0] = (char)('0' + t.hour / 10u);
    buf[1] = (char)('0' + t.hour % 10u);
    buf[2] = ':';
    buf[3] = (char)('0' + t.min / 10u);
    buf[4] = (char)('0' + t.min % 10u);
    buf[5] = ':';
    buf[6] = (char)('0' + t.sec / 10u);
    buf[7] = (char)('0' + t.sec % 10u);
    buf[8] = '\0';

    GUI_SetAsciiFont(&Disp_FontFlash_Ascii12x24);
    GUI_SetCjkFont(NULL);
    GUI_DrawString(UI_TIME_X, UI_TIME_Y, buf, GUI_BLACK, GUI_WHITE, 1);
    GUI_SetAsciiFont(fontSave);          /* 还原成进入时的字体（原来是自我赋值，等于没还原） */

    s_last = t;
}

static void Ui_Init(void)
{
    uint16_t id;

    /* 整片 GRAM 刷白：面板可见但从未写过的区域也刷掉，避免露出未初始化的条纹
     * （这是驱动特有扩展，图形库通用接口里没有） */
    Disp_ILI9327_ClearPanel(GUI_WHITE);

    /* 1. 实时时钟（大字 HH:MM:SS，Ui_Update 里每秒刷新） */
    Ui_UpdateClock(1u);

    /* 2. 电池/充电电压（数值在 Ui_Update 里刷新）
     *    PC0 = 充电/电源，PC1 = 电池；两个 33K 分压 => 实际电压 = ADC 电压 x2 */
    GUI_DrawString(UI_ROW_X, UI_BAT_Y, "BAT:", GUI_BLACK, GUI_WHITE, 1);
    GUI_DrawString(UI_ROW_X, UI_CHG_Y, "CHG:", GUI_BLACK, GUI_WHITE, 1);
    GUI_DrawString(UI_UNIT_X, UI_BAT_Y, "mV", GUI_BLACK, GUI_WHITE, 1);
    GUI_DrawString(UI_UNIT_X, UI_CHG_Y, "mV", GUI_BLACK, GUI_WHITE, 1);

    /* 3. 按键信息：开机键状态 + TM1668 矩阵键值 */
    GUI_DrawString(UI_ROW_X, UI_KEY_Y, "KEY:", GUI_BLACK, GUI_WHITE, 1);
    GUI_DrawString(UI_VAL_X, UI_KEY_Y, "RELEASED", GUI_GREEN, GUI_WHITE, 1);
    GUI_DrawString(UI_ROW_X, UI_TMKEY_Y, "TMKEY:", GUI_GRAY, GUI_WHITE, 1);
    GUI_DrawString(UI_VAL_X, UI_TMKEY_Y, "--", GUI_GRAY, GUI_WHITE, 1);

    id = Disp_ILI9327_ReadID();
    Debug_Printf("[LCD] controller ID(0x04) = 0x%04X\r\n", id);
}

static uint32_t s_lastAdcTick = 0;
static uint16_t s_lastBat = 0xFFFF, s_lastChg = 0xFFFF;
static uint8_t  s_adcDbgCnt = 0;

/**
  * @brief  ADC 自愈：未就绪或采样报错时，最多每 2s 重新校准一次
  *         （复位后第一次校准在电源/背光还没稳定时可能不准，这里补一次）
  */
static void Adc_Maintain(void)
{
    static uint32_t s_recalTick = 0;

    if (PowerAdc_IsReady() != 0u && PowerAdc_LastError() == 0u) return;
    if ((uint32_t)(HAL_GetTick() - s_recalTick) < 2000u) return;

    s_recalTick = HAL_GetTick();
    Debug_Printf("[ADC] not ready (err=%u) -> recalibrate...\r\n",
                 (unsigned int)PowerAdc_LastError());
    if (PowerAdc_Init() == 0u)
    {
        Debug_Printf("[ADC] recalibrate ok\r\n");
        s_lastBat = 0xFFFF;      /* 强制下一轮重画数值 */
        s_lastChg = 0xFFFF;
    }
}

static void Ui_Update(void)
{
    uint32_t now     = HAL_GetTick();
    uint8_t  pressed = POWER_KeyIsPressed();
    uint16_t bat, chg;

    Adc_Maintain();          /* ADC 掉链子时自动重校准 */

    /* 电源电压：每 500ms 采一次，只在数值变化时才重画 */
    if ((uint32_t)(now - s_lastAdcTick) >= 500u)
    {
        s_lastAdcTick = now;

        bat = (uint16_t)PowerAdc_BatMv();
        chg = (uint16_t)PowerAdc_PowerMv();

        /* 调试：每 2 秒打印两个通道的原始值和折算值（读的是同一组缓存，不额外采样） */
        if (++s_adcDbgCnt >= 4u)
        {
            s_adcDbgCnt = 0;
            Debug_Printf("[ADC] PC0 raw=%u -> %u mV | PC1 raw=%u -> %u mV | err=%u\r\n",
                         (unsigned int)PowerAdc_ReadRaw(PWR_ADC_CH_POWER), (unsigned int)chg,
                         (unsigned int)PowerAdc_ReadRaw(PWR_ADC_CH_BAT),   (unsigned int)bat,
                         (unsigned int)PowerAdc_LastError());
        }

        if (bat != s_lastBat)
        {
            s_lastBat = bat;
            GUI_DrawNumber(UI_VAL_X, UI_BAT_Y, (int32_t)bat, 5, GUI_BLACK, GUI_WHITE, 1);
        }
        if (chg != s_lastChg)
        {
            s_lastChg = chg;
            GUI_DrawNumber(UI_VAL_X, UI_CHG_Y, (int32_t)chg, 5, GUI_BLACK, GUI_WHITE, 1);
        }
    }

    /* 实时时钟（内部 500ms 节流，秒变化才重画） */
    Ui_UpdateClock(0u);

    /* 开机键状态：按下=红，松开=绿 */
    GUI_DrawString(UI_VAL_X, UI_KEY_Y, pressed ? "PRESSED " : "RELEASED",
                   pressed ? GUI_RED : GUI_GREEN, GUI_WHITE, 1);
}

/* 屏幕上显示关机提示，然后断电 */
static void Ui_ShowPowerOff(void)
{
    GUI_FillRect(0, 204, (int16_t)(GUI_Width() - 1u), (int16_t)(GUI_Height() - 1u), GUI_RED);
    GUI_DrawString(100, 216, "POWER OFF...", GUI_WHITE, GUI_RED, 2);
    GUI_SetBacklight(0);
    HAL_Delay(300);
}

static void Ui_Task(void)
{
    static uint32_t s_keyTick = 0;
    static uint16_t s_lastTmKey = 0xFFFF;

    uint32_t now = HAL_GetTick();
    uint8_t  ch;

    POWER_Poll(now);               /* 按键去抖 + 长按计时（每轮都要调用） */

    /* TM1668 按键轮询：20ms 一次（内部含去抖），按下时上报映射后的键值 */
    if ((uint32_t)(now - s_keyTick) >= 20u)
    {
        uint8_t key = TM1668_GetKey();

        s_keyTick = now;
        if (key != 0u)
        {
            Debug_Printf("[TM1668] key value = %u\r\n", (unsigned int)key);
            if ((uint16_t)key != s_lastTmKey)
            {
                s_lastTmKey = (uint16_t)key;
                GUI_DrawNumber(UI_VAL_X, UI_TMKEY_Y, (int32_t)key, 2, GUI_RED, GUI_WHITE, 1);
            }
        }
    }

    /* 长按开机键 3 秒 -> 软件断电 */
    if (POWER_LongPress(POWER_OFF_HOLD_MS))
    {
        Debug_Printf("[PWR] long press %u ms -> POW_SOFT_EN = LOW\r\n",
                     (unsigned int)POWER_PressDuration());
        Ui_ShowPowerOff();
        POWER_Shutdown();          /* 拉低 POW_SOFT_EN 并保持，系统断电 */
    }

    /* 备用验收手段：串口发一个小写 o 也能关机（用来单独验证关机通路） */
    if (HAL_UART_Receive(&huart1, &ch, 1, 0) == HAL_OK)
    {
        if (ch == 'o' || ch == 'O')
        {
            Debug_Printf("[PWR] shutdown by UART command\r\n");
            Ui_ShowPowerOff();
            POWER_Shutdown();
        }
    }

    if (now - s_lastRefresh >= 40/*DEMO_REFRESH_MS*/)
    {
        s_lastRefresh = now;
        Ui_Update();
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  /* 第一步：接管电源自锁，必须在任何初始化之前（按键松开后靠 PE0 维持供电） */
  POWER_SelfHoldEarly();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_FSMC_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_SPI3_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_USB_PCD_Init();
  /* USER CODE BEGIN 2 */
  POWER_SelfHold();       /* 再次确认 POW_SOFT_EN = 1（CubeMX 里请把它初始电平设为 High） */
  POWER_Init();           /* 按键 PE1 配置成输入 + 内部下拉（CubeMX 生成的是无上下拉） */
  Debug_Printf("\r\n[Boot] POW_SOFT_EN=H, POWER_KEY(raw)=%d (1=按下)\r\n", POWER_KeyRaw());

  /* 绑定显示设备：图形库从这里开始与具体屏幕解耦（换屏只需换这个对象） */
  if (GUI_Init(&Disp_ILI9327) != 0u)
  {
    Debug_Printf("[GUI] display init failed\r\n");
  }
  GUI_SetBacklight(90);  /* 背光（经驱动层转到 PB0 PWM） */

  Flash_Init_And_Report();/* SPI1: W25Q64 数据 Flash + SPI2: 字库 Flash */

  /* 电源电压采集：PC0(充电) / PC1(电池)，均为 33K+33K 分压，实际电压 = ADC 电压 x2 */
  if (PowerAdc_Init() == 0u)
  {
    Debug_Printf("[ADC] init ok (calibrated at reset)\r\n");
  }
  else
  {
    Debug_Printf("[ADC] init failed, err=%u\r\n", (unsigned int)PowerAdc_LastError());
  }

  /* 复位后立刻校准的那一次是在电源/背光刚起来时做的，等电源稳定后再校准一次更准 */
  HAL_Delay(50);
  if (PowerAdc_Calibrate() == 0u)
  {
    Debug_Printf("[ADC] recalibrated after power stable\r\n");
  }
  Debug_Printf("[ADC] BAT=%u mV, CHG=%u mV (div 1/%u), err=%u\r\n",
               (unsigned int)PowerAdc_BatMv(), (unsigned int)PowerAdc_PowerMv(),
               (unsigned int)PWR_ADC_DIV_DEN, (unsigned int)PowerAdc_LastError());

  Ui_Init();              /* 画第一屏（图形库 + 字库字体） */

  /* TM1668 按键扫描（K1/K2 x SEG1~SEG10 = 20 键） */
  if (TM1668_Init() == 0u)
  {
    Debug_Printf("[TM1668] init ok, 20 keys ready\r\n");
  }
  else
  {
    Debug_Printf("[TM1668] init failed\r\n");
  }

  /* PCF8563 RTC：首次上电写入基线时间；之后仅当读到的时间小于基线（掉电过）才重设 */
  if (RTC_Init() == 0u)
  {
    RTC_Time_t t;
    if (RTC_ReadTime(&t) == 0u)
    {
      Debug_Printf("[RTC] 20%02u-%02u-%02u %02u:%02u:%02u (weekday %u)\r\n",
                   (unsigned int)t.year, (unsigned int)t.month, (unsigned int)t.day,
                   (unsigned int)t.hour, (unsigned int)t.min, (unsigned int)t.sec,
                   (unsigned int)t.weekday);
    }
  }
  else
  {
    Debug_Printf("[RTC] PCF8563 not found / sync failed (check I2C1 pull-up)\r\n");
  }
  Debug_Printf("[LCD] %dx%d init done.\r\n", GUI_Width(), GUI_Height());
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    Ui_Task();            /* 刷新时间/按键状态 + 长按关机检测 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_USB;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
