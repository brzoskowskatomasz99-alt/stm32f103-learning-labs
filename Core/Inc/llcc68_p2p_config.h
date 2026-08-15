/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    llcc68_p2p_config.h
  * @brief   Compile-time TX/RX role selection and the single set of common
  *          LoRa P2P parameters shared by both roles.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LLCC68_P2P_CONFIG_H__
#define __LLCC68_P2P_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "llcc68.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported defines ----------------------------------------------------------*/
/*!< Compile-time role values */
#define LLCC68_P2P_ROLE_TX   1
#define LLCC68_P2P_ROLE_RX   2

/*!< Default workspace role: gateway. Either role may override
     LLCC68_P2P_ROLE in the compiler defines without editing this file. */
#ifndef LLCC68_P2P_ROLE
#define LLCC68_P2P_ROLE      LLCC68_P2P_ROLE_RX
#endif

/* Exported constants --------------------------------------------------------*/
/*!< Common wireless parameters - single set, both roles reference these */
/* 475.5 MHz：避开粤嵌课程默认 470.5 MHz 的 50 组同频干扰（2026-08-15 实测） */
#define LLCC68_P2P_FREQ_HZ            475500000UL
#define LLCC68_P2P_LORA_SF            LLCC68_LORA_SF9
#define LLCC68_P2P_LORA_BW            LLCC68_LORA_BW_125
#define LLCC68_P2P_LORA_CR            LLCC68_LORA_CR_4_5
#define LLCC68_P2P_PREAMBLE_SYMB      8U
#define LLCC68_P2P_HEADER_TYPE        LLCC68_LORA_PKT_EXPLICIT
#define LLCC68_P2P_CRC_IS_ON          true
#define LLCC68_P2P_INVERT_IQ_IS_ON    false
#define LLCC68_P2P_TX_PWR_DBM         22
#define LLCC68_P2P_TX_INTERVAL_MS     20000U
#define LLCC68_P2P_MAX_PAYLOAD_LEN    250U

/*!< 470.5 MHz image calibration range, shared by both roles (LLCC68 datasheet
     9.2.1, Table 9-2: use the 470-510 MHz calibration band).
     llcc68_cal_img_in_mhz takes the range in MHz. */
#define LLCC68_P2P_CAL_IMG_FREQ1_MHZ  470U
#define LLCC68_P2P_CAL_IMG_FREQ2_MHZ  510U

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __LLCC68_P2P_CONFIG_H__ */
