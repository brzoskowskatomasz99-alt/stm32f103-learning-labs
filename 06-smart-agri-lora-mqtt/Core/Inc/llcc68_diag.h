/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    llcc68_diag.h
  * @brief   This file contains the prototype for the LLCC68 reset + GetStatus
  *          diagnostic entry (run once on power-up).
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
#ifndef __LLCC68_DIAG_H__
#define __LLCC68_DIAG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

void LLCC68_Diag_RunOnce(void);

#ifdef __cplusplus
}
#endif

#endif /* __LLCC68_DIAG_H__ */
