/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
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

#if defined ( __ICCARM__ )
#  define CMSE_NS_CALL  __cmse_nonsecure_call
#  define CMSE_NS_ENTRY __cmse_nonsecure_entry
#else
#  define CMSE_NS_CALL  __attribute((cmse_nonsecure_call))
#  define CMSE_NS_ENTRY __attribute((cmse_nonsecure_entry))
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* Function pointer declaration in non-secure*/
#if defined ( __ICCARM__ )
typedef void (CMSE_NS_CALL *funcptr)(void);
#else
typedef void CMSE_NS_CALL (*funcptr)(void);
#endif

/* typedef for non-secure callback functions */
typedef funcptr funcptr_NS;

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define USER_BUTTON_Pin GPIO_PIN_13
#define USER_BUTTON_GPIO_Port GPIOC
#define VBUS_SENSE_Pin GPIO_PIN_2
#define VBUS_SENSE_GPIO_Port GPIOC
#define MACRO_KW_DETECTED_Pin GPIO_PIN_5
#define MACRO_KW_DETECTED_GPIO_Port GPIOA
#define MACRO_WIPING_Pin GPIO_PIN_6
#define MACRO_WIPING_GPIO_Port GPIOA
#define MACRO_WORKING_Pin GPIO_PIN_7
#define MACRO_WORKING_GPIO_Port GPIOA
#define LPUART1_TX_Pin GPIO_PIN_7
#define LPUART1_TX_GPIO_Port GPIOG
#define LPUART1_RX__ST_LINK_VCP_TX__Pin GPIO_PIN_8
#define LPUART1_RX__ST_LINK_VCP_TX__GPIO_Port GPIOG
#define LED_GREEN_Pin GPIO_PIN_7
#define LED_GREEN_GPIO_Port GPIOC
#define MACRO_DBG_1_Pin GPIO_PIN_8
#define MACRO_DBG_1_GPIO_Port GPIOA
#define LED_RED_Pin GPIO_PIN_9
#define LED_RED_GPIO_Port GPIOA
#define PROF0_Pin GPIO_PIN_0
#define PROF0_GPIO_Port GPIOD
#define PROF1_Pin GPIO_PIN_1
#define PROF1_GPIO_Port GPIOD
#define PROF2_Pin GPIO_PIN_2
#define PROF2_GPIO_Port GPIOD
#define PROF3_Pin GPIO_PIN_3
#define PROF3_GPIO_Port GPIOD
#define PROF4_Pin GPIO_PIN_4
#define PROF4_GPIO_Port GPIOD
#define PROF5_Pin GPIO_PIN_5
#define PROF5_GPIO_Port GPIOD
#define PROF6_Pin GPIO_PIN_6
#define PROF6_GPIO_Port GPIOD
#define PROF7_Pin GPIO_PIN_7
#define PROF7_GPIO_Port GPIOD
#define LED_BLUE_Pin GPIO_PIN_7
#define LED_BLUE_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
