/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    llcc68_hal_stm32.h
  * @brief   STM32 platform context for the LLCC68 radio driver.
  *          Implements the HAL interface declared in llcc68_hal.h (upstream).
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LLCC68_HAL_STM32_H
#define __LLCC68_HAL_STM32_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Exported types ------------------------------------------------------------*/
/**
  * @brief STM32 platform context used by the LLCC68 HAL functions.
  *        Binds the SPI handle and the NSS / RESET / BUSY GPIO lines.
  */
typedef struct
{
  SPI_HandleTypeDef *spi;      /*!< SPI peripheral used to talk to the radio */
  GPIO_TypeDef      *nss_port; /*!< NSS GPIO port                           */
  uint16_t           nss_pin;  /*!< NSS GPIO pin                            */
  GPIO_TypeDef      *reset_port; /*!< RESET GPIO port                      */
  uint16_t           reset_pin;  /*!< RESET GPIO pin                       */
  GPIO_TypeDef      *busy_port;  /*!< BUSY GPIO port                       */
  uint16_t           busy_pin;   /*!< BUSY GPIO pin                        */
  uint32_t           spi_timeout_ms;  /*!< Timeout for SPI operations, in ms */
  uint32_t           busy_timeout_ms; /*!< Timeout for BUSY wait, in ms      */
} llcc68_hal_stm32_context_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/
/**
  * @brief Default context, bound to hspi1 and the LORA_NSS / LORA_RESET /
  *        LORA_BUSY pins as configured in the CubeMX project.
  */
extern const llcc68_hal_stm32_context_t llcc68_hal_stm32_context;

/* Exported functions prototypes ---------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* __LLCC68_HAL_STM32_H */
