/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define KEY_STB_Pin GPIO_PIN_2
#define KEY_STB_GPIO_Port GPIOE
#define KEY_CLK_Pin GPIO_PIN_3
#define KEY_CLK_GPIO_Port GPIOE
#define DIO_Pin GPIO_PIN_4
#define DIO_GPIO_Port GPIOE
#define FN_CS_Pin GPIO_PIN_13
#define FN_CS_GPIO_Port GPIOC
#define ADC_POWER_Pin GPIO_PIN_0
#define ADC_POWER_GPIO_Port GPIOC
#define ADC_BAT_Pin GPIO_PIN_1
#define ADC_BAT_GPIO_Port GPIOC
#define FLASH1_CS_Pin GPIO_PIN_4
#define FLASH1_CS_GPIO_Port GPIOA
#define MF522_IRQ_Pin GPIO_PIN_0
#define MF522_IRQ_GPIO_Port GPIOB
#define BEEP_Pin GPIO_PIN_1
#define BEEP_GPIO_Port GPIOB
#define FLASH2_CS_Pin GPIO_PIN_12
#define FLASH2_CS_GPIO_Port GPIOB
#define M24_INT_Pin GPIO_PIN_12
#define M24_INT_GPIO_Port GPIOD
#define M24_RF_CS_Pin GPIO_PIN_7
#define M24_RF_CS_GPIO_Port GPIOC
#define LCD_BL_Pin GPIO_PIN_8
#define LCD_BL_GPIO_Port GPIOC
#define LCD_RESET_Pin GPIO_PIN_9
#define LCD_RESET_GPIO_Port GPIOC
#define STM32_USB_EN_Pin GPIO_PIN_8
#define STM32_USB_EN_GPIO_Port GPIOA
#define FN_SET_Pin GPIO_PIN_12
#define FN_SET_GPIO_Port GPIOC
#define MF522_NPD_Pin GPIO_PIN_6
#define MF522_NPD_GPIO_Port GPIOB
#define POW_SOFT_EN_Pin GPIO_PIN_0
#define POW_SOFT_EN_GPIO_Port GPIOE
#define POWER_KEY_TEST_Pin GPIO_PIN_1
#define POWER_KEY_TEST_GPIO_Port GPIOE
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
