/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    llcc68_diag.c
  * @brief   LLCC68 reset + GetStatus diagnostic, run once after power-up.
  *          Diagnostic output is printed over USART1 via printf (fputc retarget).
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "llcc68_diag.h"
#include "llcc68.h"
#include "llcc68_hal_stm32.h"
#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
/* Storage for the radio status structure returned by llcc68_get_status. */
static llcc68_chip_status_t llcc68_diag_chip_status;

/* Private function prototypes -----------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/* Public functions ----------------------------------------------------------*/
/**
  * @brief  Run the LLCC68 reset + GetStatus diagnostic once.
  * @note   Called a single time after GPIO/SPI1/USART1 initialization, before
  *         the main application loop. Uses the default HAL context
  *         (llcc68_hal_stm32_context) and the driver's own bounded timeouts.
  *         On reset failure the function returns without calling GetStatus.
  *         No RF/LoRa parameter is configured here.
  * @retval None
  */
void LLCC68_Diag_RunOnce(void)
{
  llcc68_status_t status = LLCC68_STATUS_ERROR;

  printf("[LLCC68] TEST START\r\n");

  status = llcc68_reset(&llcc68_hal_stm32_context);
  if (status != LLCC68_STATUS_OK)
  {
    printf("[LLCC68] RESET FAIL\r\n");
    return;
  }
  printf("[LLCC68] RESET OK\r\n");

  status = llcc68_get_status(&llcc68_hal_stm32_context, &llcc68_diag_chip_status);
  if (status != LLCC68_STATUS_OK)
  {
    printf("[LLCC68] GET_STATUS FAIL\r\n");
  }
  else
  {
    printf("[LLCC68] GET_STATUS OK\r\n");
    printf("[LLCC68] CHIP_MODE=%d CMD_STATUS=%d\r\n",
           (int)llcc68_diag_chip_status.chip_mode,
           (int)llcc68_diag_chip_status.cmd_status);
  }

  printf("[LLCC68] TEST END\r\n");
}
