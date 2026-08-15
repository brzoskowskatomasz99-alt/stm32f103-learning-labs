/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    llcc68_p2p.h
  * @brief   Minimal LLCC68 point-to-point demo: compile-time TX/RX role.
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __LLCC68_P2P_H__
#define __LLCC68_P2P_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include "protocol_lora.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported functions prototypes ---------------------------------------------*/
/**
  * @brief  Initialize the radio for the compiled role (TX or RX).
  * @note   All radio commands are checked; on failure the failing stage is
  *         printed and false is returned (no further RF operation is done).
  * @retval true on success, false on failure
  */
bool LLCC68_P2P_Init(void);

/**
  * @brief  Periodic processing: bounded polling only, no infinite wait.
  * @note   TX: sends one short packet every LLCC68_P2P_TX_INTERVAL_MS.
  *         RX: keeps continuous reception, handles RX_DONE / CRC error /
  *         timeout and resumes reception. No DIO1-based radio callback.
  */
void LLCC68_P2P_Process(void);

/** Queue one encoded protocol frame for the next half-duplex TX opportunity. */
bool LLCC68_P2P_QueueFrame(const ProtocolLoraFrame *frame);

/** True while a queued frame is waiting to be sent or being sent. */
bool LLCC68_P2P_IsTxPending(void);

typedef struct
{
  bool valid;
  int8_t rssi_dbm;
  int8_t snr_db;
} LLCC68P2PRxMeta;

/** Take the latest valid protocol frame and its matching link metrics. */
bool LLCC68_P2P_TakeReceivedFrameWithMeta(ProtocolLoraFrame *frame,
                                          LLCC68P2PRxMeta *meta);

/** Compatibility wrapper when link metrics are not needed. */
bool LLCC68_P2P_TakeReceivedFrame(ProtocolLoraFrame *frame);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __LLCC68_P2P_H__ */
