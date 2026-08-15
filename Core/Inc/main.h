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
#define LORA_NSS_Pin GPIO_PIN_4
#define LORA_NSS_GPIO_Port GPIOA
#define LORA_BUSY_Pin GPIO_PIN_0
#define LORA_BUSY_GPIO_Port GPIOB
#define LORA_RESET_Pin GPIO_PIN_1
#define LORA_RESET_GPIO_Port GPIOB
#define LORA_DIO1_Pin GPIO_PIN_10
#define LORA_DIO1_GPIO_Port GPIOB
#define MOTOR_Pin GPIO_PIN_8
#define MOTOR_GPIO_Port GPIOB
#define BEEP_Pin GPIO_PIN_9
#define BEEP_GPIO_Port GPIOB
#define LED3_Pin GPIO_PIN_14
#define LED3_GPIO_Port GPIOB
#define LED2_Pin GPIO_PIN_15
#define LED2_GPIO_Port GPIOB
#define RELAY_Pin GPIO_PIN_14
#define RELAY_GPIO_Port GPIOC

/* USER CODE BEGIN Private defines */
/* Terminal keys and compatibility LED alias used by legacy callbacks. */
#define KEY1_Pin       GPIO_PIN_12
#define KEY1_GPIO_Port GPIOB
#define KEY2_Pin       GPIO_PIN_13
#define KEY2_GPIO_Port GPIOB
#define LED_Pin        LED3_Pin
#define LED_GPIO_Port  LED3_GPIO_Port

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
