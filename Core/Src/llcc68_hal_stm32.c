/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    llcc68_hal_stm32.c
  * @brief   STM32 HAL platform adaptation for the LLCC68 radio driver.
  *          Implements the four interfaces declared in llcc68_hal.h:
  *          llcc68_hal_write / llcc68_hal_read / llcc68_hal_reset /
  *          llcc68_hal_wakeup.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "llcc68_hal.h"
#include "llcc68_hal_stm32.h"
#include "main.h"
#include "spi.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/
#define LLCC68_HAL_STM32_RESET_PULSE_MS  10U    /*!< Reset pulse width, at least 10 ms */
#define LLCC68_HAL_STM32_SPI_TIMEOUT_MS  1000U  /*!< Default SPI operation timeout, in ms */
#define LLCC68_HAL_STM32_BUSY_TIMEOUT_MS 1000U  /*!< Default BUSY line wait timeout, in ms */
#define LLCC68_HAL_STM32_SPI_CHUNK_SIZE  16U    /*!< Fixed chunk size for long SPI reads */

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
/* Fixed, initialized dummy bytes used to generate SPI read clocks. */
static const uint8_t llcc68_hal_stm32_dummy_bytes[LLCC68_HAL_STM32_SPI_CHUNK_SIZE] = { LLCC68_NOP };

/* Default platform context: binds hspi1 and the LoRa control pins. */
const llcc68_hal_stm32_context_t llcc68_hal_stm32_context = {
  &hspi1,
  LORA_NSS_GPIO_Port,
  LORA_NSS_Pin,
  LORA_RESET_GPIO_Port,
  LORA_RESET_Pin,
  LORA_BUSY_GPIO_Port,
  LORA_BUSY_Pin,
  LLCC68_HAL_STM32_SPI_TIMEOUT_MS,
  LLCC68_HAL_STM32_BUSY_TIMEOUT_MS
};

/* Private function prototypes -----------------------------------------------*/
static llcc68_hal_status_t llcc68_hal_stm32_wait_busy_low(const llcc68_hal_stm32_context_t *ctx);

/* Private functions ---------------------------------------------------------*/
/**
  * @brief Wait until the radio BUSY line is low, with a bounded timeout.
  * @param ctx Platform context
  * @retval LLCC68_HAL_STATUS_OK if BUSY went low
  * @retval LLCC68_HAL_STATUS_ERROR on timeout
  */
static llcc68_hal_status_t llcc68_hal_stm32_wait_busy_low(const llcc68_hal_stm32_context_t *ctx)
{
  uint32_t start_tick = HAL_GetTick();

  while (HAL_GPIO_ReadPin(ctx->busy_port, ctx->busy_pin) == GPIO_PIN_SET)
  {
    if ((HAL_GetTick() - start_tick) >= ctx->busy_timeout_ms)
    {
      return LLCC68_HAL_STATUS_ERROR;
    }
  }

  return LLCC68_HAL_STATUS_OK;
}

/* Public functions ----------------------------------------------------------*/
/**
  * @brief Radio data transfer - write (command, optionally followed by data).
  * @param context       Platform context (llcc68_hal_stm32_context_t)
  * @param command       Command buffer
  * @param command_length Command buffer size
  * @param data          Optional data buffer (may be NULL when data_length is 0)
  * @param data_length   Data buffer size
  * @retval LLCC68_HAL_STATUS_OK on success, LLCC68_HAL_STATUS_ERROR otherwise
  */
llcc68_hal_status_t llcc68_hal_write(const void *context, const uint8_t *command, const uint16_t command_length,
                                     const uint8_t *data, const uint16_t data_length)
{
  const llcc68_hal_stm32_context_t *ctx = (const llcc68_hal_stm32_context_t *)context;
  HAL_StatusTypeDef hal_status = HAL_ERROR;

  if ((ctx == NULL) || (ctx->spi == NULL) || (command == NULL) || (command_length == 0U))
  {
    return LLCC68_HAL_STATUS_ERROR;
  }

  if (llcc68_hal_stm32_wait_busy_low(ctx) != LLCC68_HAL_STATUS_OK)
  {
    return LLCC68_HAL_STATUS_ERROR;
  }

  HAL_GPIO_WritePin(ctx->nss_port, ctx->nss_pin, GPIO_PIN_RESET);

  hal_status = HAL_SPI_Transmit(ctx->spi, (uint8_t *)command, command_length, ctx->spi_timeout_ms);
  if ((hal_status == HAL_OK) && (data_length > 0U) && (data != NULL))
  {
    hal_status = HAL_SPI_Transmit(ctx->spi, (uint8_t *)data, data_length, ctx->spi_timeout_ms);
  }

  HAL_GPIO_WritePin(ctx->nss_port, ctx->nss_pin, GPIO_PIN_SET);

  return (hal_status == HAL_OK) ? LLCC68_HAL_STATUS_OK : LLCC68_HAL_STATUS_ERROR;
}

/**
  * @brief Radio data transfer - read (command, then data received on SPI).
  * @param context       Platform context (llcc68_hal_stm32_context_t)
  * @param command       Command buffer
  * @param command_length Command buffer size
  * @param data          Buffer receiving the data
  * @param data_length   Number of bytes to receive
  * @retval LLCC68_HAL_STATUS_OK on success, LLCC68_HAL_STATUS_ERROR otherwise
  */
llcc68_hal_status_t llcc68_hal_read(const void *context, const uint8_t *command, const uint16_t command_length,
                                    uint8_t *data, const uint16_t data_length)
{
  const llcc68_hal_stm32_context_t *ctx = (const llcc68_hal_stm32_context_t *)context;
  HAL_StatusTypeDef hal_status = HAL_ERROR;
  uint16_t offset = 0;

  if ((ctx == NULL) || (ctx->spi == NULL) || (command == NULL) || (command_length == 0U) ||
      (data == NULL) || (data_length == 0U))
  {
    return LLCC68_HAL_STATUS_ERROR;
  }

  if (llcc68_hal_stm32_wait_busy_low(ctx) != LLCC68_HAL_STATUS_OK)
  {
    return LLCC68_HAL_STATUS_ERROR;
  }

  HAL_GPIO_WritePin(ctx->nss_port, ctx->nss_pin, GPIO_PIN_RESET);

  hal_status = HAL_SPI_Transmit(ctx->spi, (uint8_t *)command, command_length, ctx->spi_timeout_ms);
  if (hal_status == HAL_OK)
  {
    /* Generate the read clocks with the fixed, initialized dummy buffer.
       Long reads are segmented into fixed-size chunks (no uncontrolled VLA). */
    while ((hal_status == HAL_OK) && (offset < data_length))
    {
      uint16_t chunk = data_length - offset;

      if (chunk > LLCC68_HAL_STM32_SPI_CHUNK_SIZE)
      {
        chunk = LLCC68_HAL_STM32_SPI_CHUNK_SIZE;
      }
      hal_status = HAL_SPI_TransmitReceive(ctx->spi, (uint8_t *)llcc68_hal_stm32_dummy_bytes, &data[offset], chunk,
                                           ctx->spi_timeout_ms);
      offset += chunk;
    }
  }

  HAL_GPIO_WritePin(ctx->nss_port, ctx->nss_pin, GPIO_PIN_SET);

  return (hal_status == HAL_OK) ? LLCC68_HAL_STATUS_OK : LLCC68_HAL_STATUS_ERROR;
}

/**
  * @brief Reset the radio (RESET low >= 10 ms, then high, then wait BUSY low).
  * @param context Platform context (llcc68_hal_stm32_context_t)
  * @retval LLCC68_HAL_STATUS_OK on success, LLCC68_HAL_STATUS_ERROR otherwise
  */
llcc68_hal_status_t llcc68_hal_reset(const void *context)
{
  const llcc68_hal_stm32_context_t *ctx = (const llcc68_hal_stm32_context_t *)context;

  if ((ctx == NULL) || (ctx->spi == NULL) || (ctx->nss_port == NULL) || (ctx->reset_port == NULL) ||
      (ctx->busy_port == NULL))
  {
    return LLCC68_HAL_STATUS_ERROR;
  }

  /* Keep NSS high (radio deselected) during the reset sequence. */
  HAL_GPIO_WritePin(ctx->nss_port, ctx->nss_pin, GPIO_PIN_SET);

  /* RESET low for at least 10 ms, then released high. */
  HAL_GPIO_WritePin(ctx->reset_port, ctx->reset_pin, GPIO_PIN_RESET);
  HAL_Delay(LLCC68_HAL_STM32_RESET_PULSE_MS);
  HAL_GPIO_WritePin(ctx->reset_port, ctx->reset_pin, GPIO_PIN_SET);

  return llcc68_hal_stm32_wait_busy_low(ctx);
}

/**
  * @brief Wake the radio up from sleep mode.
  *
  * The chip in sleep mode does not drive BUSY (pulled high on the module).
  * Asserting NSS low wakes it; BUSY goes low once the chip is ready. NSS is
  * then released high. No RF or LoRa parameter is configured here.
  *
  * @param context Platform context (llcc68_hal_stm32_context_t)
  * @retval LLCC68_HAL_STATUS_OK on success, LLCC68_HAL_STATUS_ERROR otherwise
  */
llcc68_hal_status_t llcc68_hal_wakeup(const void *context)
{
  const llcc68_hal_stm32_context_t *ctx = (const llcc68_hal_stm32_context_t *)context;
  llcc68_hal_status_t status = LLCC68_HAL_STATUS_ERROR;

  if ((ctx == NULL) || (ctx->spi == NULL) || (ctx->nss_port == NULL) || (ctx->busy_port == NULL))
  {
    return LLCC68_HAL_STATUS_ERROR;
  }

  HAL_GPIO_WritePin(ctx->nss_port, ctx->nss_pin, GPIO_PIN_RESET);
  status = llcc68_hal_stm32_wait_busy_low(ctx);
  HAL_GPIO_WritePin(ctx->nss_port, ctx->nss_pin, GPIO_PIN_SET);

  return status;
}
