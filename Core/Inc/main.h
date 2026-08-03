/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
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
#define LIGHT_ADC1_IN0_Pin GPIO_PIN_0
#define LIGHT_ADC1_IN0_GPIO_Port GPIOA
#define SOIL_ADC1_IN1_Pin GPIO_PIN_1
#define SOIL_ADC1_IN1_GPIO_Port GPIOA
#define LED3_Pin GPIO_PIN_15
#define LED3_GPIO_Port GPIOB
#define BEEP_Pin GPIO_PIN_9
#define BEEP_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
/* 按键和LED引脚重映射（匹配ISR_callback.c中的名字） */
#define KEY1_Pin       GPIO_PIN_12   // 假设按键接在PB12（根据你之前截图中的SW1定义）
#define KEY1_GPIO_Port GPIOB
// 把这两行改成指向 LED3（PB15）
#define LED_Pin        LED3_Pin      
#define LED_GPIO_Port  LED3_GPIO_Port

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
