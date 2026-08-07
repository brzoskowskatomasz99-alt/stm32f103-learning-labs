/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    llcc68_p2p.c
  * @brief   Minimal LLCC68 point-to-point demo. The role (TX or RX) is chosen
  *          at compile time through llcc68_p2p_config.h. All radio polling is
  *          bounded; no DIO1-based radio callback is used.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "llcc68_p2p.h"
#include "llcc68_p2p_config.h"
#include "llcc68.h"
#include "llcc68_hal_stm32.h"
#include <stdio.h>

/* Private typedef -----------------------------------------------------------*/
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
typedef enum
{
  LLCC68_P2P_TX_IDLE,          /*!< Waiting for the next interval start      */
  LLCC68_P2P_TX_WAIT_INTERVAL, /*!< Waiting until the TX interval elapses    */
  LLCC68_P2P_TX_WAIT_DONE      /*!< Waiting for TX_DONE (bounded, tick-based) */
} llcc68_p2p_tx_state_t;
#endif

/* Private define ------------------------------------------------------------*/
/*!< Bounded wait for TX completion: generous upper bound for the classroom
     demo (actual on-air time at SF7/125 kHz is far below this) */
#define LLCC68_P2P_TX_DONE_TIMEOUT_MS  5000U

/*!< RX payload buffer: max payload + terminating NUL */
#define LLCC68_P2P_RX_BUFFER_SIZE      ( LLCC68_P2P_MAX_PAYLOAD_LEN + 1U )

/*!< Short ASCII packet prefix */
#define LLCC68_P2P_TX_PREFIX           "LLCC68-P2P:"

/* Private macro -------------------------------------------------------------*/
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
#define LLCC68_P2P_ROLE_STR "TX"
#else
#define LLCC68_P2P_ROLE_STR "RX"
#endif

/* Private variables ---------------------------------------------------------*/
static bool llcc68_p2p_initialized = false;

/*!< Single set of common LoRa packet parameters, shared by both roles.
     The payload length is refreshed by the TX path before each send. */
static const llcc68_pkt_params_lora_t llcc68_p2p_pkt_params = {
  LLCC68_P2P_PREAMBLE_SYMB,
  LLCC68_P2P_HEADER_TYPE,
  LLCC68_P2P_MAX_PAYLOAD_LEN,
  LLCC68_P2P_CRC_IS_ON,
  LLCC68_P2P_INVERT_IQ_IS_ON
};

#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
static uint32_t llcc68_p2p_tx_seq = 1U;                    /*!< Sequence number, starts at 1 */
static uint8_t llcc68_p2p_tx_buffer[LLCC68_P2P_MAX_PAYLOAD_LEN];
static llcc68_p2p_tx_state_t llcc68_p2p_tx_state = LLCC68_P2P_TX_IDLE;
static uint32_t llcc68_p2p_tx_tick = 0U;
#elif ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_RX )
static uint8_t llcc68_p2p_rx_buffer[LLCC68_P2P_RX_BUFFER_SIZE];
static bool llcc68_p2p_rx_started = false;
#else
#error "LLCC68_P2P_ROLE must be LLCC68_P2P_ROLE_TX or LLCC68_P2P_ROLE_RX"
#endif

/* Private function prototypes -----------------------------------------------*/
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
static void llcc68_p2p_tx_send(void);
static void llcc68_p2p_process_tx(void);
#else
static void llcc68_p2p_process_rx(void);
#endif

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Common radio configuration for both roles, stage by stage.
  * @note   Every stage is checked; on failure the stage name is printed and
  *         false is returned without any further RF operation.
  * @retval true on success, false on failure
  */
static bool llcc68_p2p_configure_radio(void)
{
  llcc68_chip_status_t chip_status;
  llcc68_irq_mask_t irq_mask;
  const llcc68_mod_params_lora_t mod_params = {
    LLCC68_P2P_LORA_SF,
    LLCC68_P2P_LORA_BW,
    LLCC68_P2P_LORA_CR,
    0U  /*!< Low DataRate Optimization: not required at SF7 / 125 kHz */
  };

  if ( llcc68_reset( &llcc68_hal_stm32_context ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] INIT FAIL RESET\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }
  if ( llcc68_get_status( &llcc68_hal_stm32_context, &chip_status ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] INIT FAIL GET_STATUS\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }
  if ( llcc68_set_standby( &llcc68_hal_stm32_context, LLCC68_STANDBY_CFG_RC ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] INIT FAIL STANDBY\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }
  /* Enable the DIO2 RF switch control: required by the Ra-01SC module whose
     internal RF switch is driven by DIO2 (datasheet 8.3.2 / 13.3.5). Without
     it, TX/RX RF paths stay disconnected while the chip still reports TX_DONE. */
  if ( llcc68_set_dio2_as_rf_sw_ctrl( &llcc68_hal_stm32_context, true ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] DIO2 RF SWITCH FAIL\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }
  /* Image calibration for the 433 MHz band (430-440 MHz, datasheet 9.2.1
     Table 9-2); must be done before setting the RF frequency. */
  if ( llcc68_cal_img_in_mhz( &llcc68_hal_stm32_context, LLCC68_P2P_CAL_IMG_FREQ1_MHZ,
                              LLCC68_P2P_CAL_IMG_FREQ2_MHZ ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] IMAGE CAL FAIL\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }
  if ( llcc68_set_pkt_type( &llcc68_hal_stm32_context, LLCC68_PKT_TYPE_LORA ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] INIT FAIL SET_PKT_TYPE\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }
  if ( llcc68_set_rf_freq( &llcc68_hal_stm32_context, LLCC68_P2P_FREQ_HZ ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] INIT FAIL SET_FREQ\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }
  if ( llcc68_set_lora_mod_params( &llcc68_hal_stm32_context, &mod_params ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] INIT FAIL SET_MOD_PARAMS\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }
  if ( llcc68_set_lora_pkt_params( &llcc68_hal_stm32_context, &llcc68_p2p_pkt_params ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] INIT FAIL SET_PKT_PARAMS\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }
  if ( llcc68_set_buffer_base_address( &llcc68_hal_stm32_context, 0U, 0U ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] INIT FAIL SET_BUFFER_BASE\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
  if ( llcc68_set_tx_params( &llcc68_hal_stm32_context, ( int8_t )LLCC68_P2P_TX_PWR_DBM,
                             LLCC68_RAMP_40_US ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][TX] INIT FAIL SET_TX_PARAMS\r\n" );
    return false;
  }
#endif

  /* Clear any stale IRQ, then enable the system-level IRQs needed by the
     compiled role. DIO1 is not used (dio1 mask = 0): polling is preferred. */
  if ( llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_ALL ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] INIT FAIL CLEAR_IRQ\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
  irq_mask = LLCC68_IRQ_TX_DONE | LLCC68_IRQ_TIMEOUT;
#else
  /* RX_DONE / CRC_ERROR / TIMEOUT handling plus preamble and header
     diagnostics (one print per event, used to verify the RF path). */
  irq_mask = LLCC68_IRQ_RX_DONE | LLCC68_IRQ_CRC_ERROR | LLCC68_IRQ_TIMEOUT |
             LLCC68_IRQ_PREAMBLE_DETECTED | LLCC68_IRQ_HEADER_VALID | LLCC68_IRQ_HEADER_ERROR;
#endif
  if ( llcc68_set_dio_irq_params( &llcc68_hal_stm32_context, irq_mask, 0U, 0U, 0U ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] INIT FAIL SET_IRQ_PARAMS\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }

  return true;
}

#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
/**
  * @brief  Build the short ASCII packet, load it into the radio buffer and
  *         start the transmission.
  * @note   Called from the TX state machine; on any failure prints an error
  *         and returns to the idle state (no infinite wait).
  */
static void llcc68_p2p_tx_send(void)
{
  llcc68_pkt_params_lora_t pkt_params = llcc68_p2p_pkt_params;
  size_t payload_len;

  payload_len = ( size_t )sprintf( ( char * )llcc68_p2p_tx_buffer, LLCC68_P2P_TX_PREFIX "%lu",
                                   ( unsigned long )llcc68_p2p_tx_seq );
  if ( payload_len > LLCC68_P2P_MAX_PAYLOAD_LEN )
  {
    payload_len = LLCC68_P2P_MAX_PAYLOAD_LEN;
  }
  pkt_params.pld_len_in_bytes = ( uint8_t )payload_len;

  if ( ( llcc68_set_lora_pkt_params( &llcc68_hal_stm32_context, &pkt_params ) != LLCC68_STATUS_OK ) ||
       ( llcc68_write_buffer( &llcc68_hal_stm32_context, 0U, llcc68_p2p_tx_buffer, ( uint8_t )payload_len ) !=
         LLCC68_STATUS_OK ) ||
       ( llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_ALL ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_tx( &llcc68_hal_stm32_context, 0U ) != LLCC68_STATUS_OK ) )
  {
    printf( "[P2P][TX] TX START FAIL\r\n" );
    llcc68_p2p_tx_state = LLCC68_P2P_TX_IDLE;
    return;
  }

  llcc68_p2p_tx_tick = HAL_GetTick();
  llcc68_p2p_tx_state = LLCC68_P2P_TX_WAIT_DONE;
}

/**
  * @brief  TX role state machine: interval wait, then bounded TX_DONE poll.
  * @note   No blocking loop: one step per Process() call, timeout tick-based.
  */
static void llcc68_p2p_process_tx(void)
{
  llcc68_irq_mask_t irq = LLCC68_IRQ_NONE;

  switch ( llcc68_p2p_tx_state )
  {
    case LLCC68_P2P_TX_IDLE:
      llcc68_p2p_tx_tick = HAL_GetTick();
      llcc68_p2p_tx_state = LLCC68_P2P_TX_WAIT_INTERVAL;
      break;

    case LLCC68_P2P_TX_WAIT_INTERVAL:
      if ( ( HAL_GetTick( ) - llcc68_p2p_tx_tick ) >= LLCC68_P2P_TX_INTERVAL_MS )
      {
        llcc68_p2p_tx_send( );
      }
      break;

    case LLCC68_P2P_TX_WAIT_DONE:
      if ( llcc68_get_irq_status( &llcc68_hal_stm32_context, &irq ) != LLCC68_STATUS_OK )
      {
        printf( "[P2P][TX] TX FAIL GET_IRQ\r\n" );
        llcc68_p2p_tx_state = LLCC68_P2P_TX_IDLE;
      }
      else if ( ( irq & LLCC68_IRQ_TX_DONE ) != 0 )
      {
        ( void )llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_TX_DONE );
        printf( "[P2P][TX] SENT SEQ=%lu\r\n", ( unsigned long )llcc68_p2p_tx_seq );
        llcc68_p2p_tx_seq++;
        llcc68_p2p_tx_state = LLCC68_P2P_TX_IDLE;
      }
      else if ( ( ( irq & LLCC68_IRQ_TIMEOUT ) != 0 ) ||
                ( ( HAL_GetTick( ) - llcc68_p2p_tx_tick ) >= LLCC68_P2P_TX_DONE_TIMEOUT_MS ) )
      {
        ( void )llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_ALL );
        printf( "[P2P][TX] TX FAIL TIMEOUT\r\n" );
        llcc68_p2p_tx_state = LLCC68_P2P_TX_IDLE;
      }
      break;
  }
}
#else /* LLCC68_P2P_ROLE_RX */
/**
  * @brief  RX role: continuous reception with bounded IRQ polling.
  * @note   Handles RX_DONE (read payload length + buffer + packet status),
  *         CRC error and timeout; every path resumes reception. A single
  *         failure never exits the receiver permanently.
  */
static void llcc68_p2p_process_rx(void)
{
  llcc68_irq_mask_t irq = LLCC68_IRQ_NONE;
  llcc68_rx_buffer_status_t rx_status;
  llcc68_pkt_status_lora_t pkt_status;
  uint8_t payload_len;

  if ( llcc68_p2p_rx_started != true )
  {
    if ( llcc68_set_rx_with_timeout_in_rtc_step( &llcc68_hal_stm32_context, LLCC68_RX_CONTINUOUS ) !=
         LLCC68_STATUS_OK )
    {
      printf( "[P2P][RX] RX START FAIL\r\n" );
      return; /* retried on the next Process() call */
    }
    llcc68_p2p_rx_started = true;
  }

  if ( llcc68_get_irq_status( &llcc68_hal_stm32_context, &irq ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][RX] GET_IRQ FAIL\r\n" );
    return;
  }

  if ( ( irq & LLCC68_IRQ_RX_DONE ) != 0 )
  {
    if ( llcc68_get_rx_buffer_status( &llcc68_hal_stm32_context, &rx_status ) != LLCC68_STATUS_OK )
    {
      printf( "[P2P][RX] GET_RX_STATUS FAIL\r\n" );
    }
    else
    {
      payload_len = rx_status.pld_len_in_bytes;
      if ( payload_len > LLCC68_P2P_MAX_PAYLOAD_LEN )
      {
        payload_len = LLCC68_P2P_MAX_PAYLOAD_LEN;
      }
      if ( llcc68_read_buffer( &llcc68_hal_stm32_context, rx_status.buffer_start_pointer, llcc68_p2p_rx_buffer,
                               payload_len ) != LLCC68_STATUS_OK )
      {
        printf( "[P2P][RX] READ_BUFFER FAIL\r\n" );
      }
      else
      {
        llcc68_p2p_rx_buffer[payload_len] = '\0';
        printf( "[P2P][RX] RECV \"%s\"\r\n", ( char * )llcc68_p2p_rx_buffer );
        if ( llcc68_get_lora_pkt_status( &llcc68_hal_stm32_context, &pkt_status ) == LLCC68_STATUS_OK )
        {
          printf( "[P2P][RX] RSSI=%d dBm SNR=%d dB\r\n", ( int )pkt_status.rssi_pkt_in_dbm,
                  ( int )pkt_status.snr_pkt_in_db );
        }
        else
        {
          printf( "[P2P][RX] GET_PKT_STATUS FAIL\r\n" );
        }
      }
    }
    ( void )llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_RX_DONE );
  }
  else if ( ( irq & LLCC68_IRQ_CRC_ERROR ) != 0 )
  {
    printf( "[P2P][RX] CRC ERROR\r\n" );
    ( void )llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_CRC_ERROR );
  }
  else if ( ( irq & LLCC68_IRQ_TIMEOUT ) != 0 )
  {
    printf( "[P2P][RX] RX TIMEOUT\r\n" );
    ( void )llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_TIMEOUT );
    llcc68_p2p_rx_started = false; /* re-enter reception on the next Process() call */
  }

  /* Preamble / header diagnostics: independent checks so a diagnostic bit
     present in the same IRQ read as RX_DONE never skips payload handling
     above. Each bit is cleared after one print (event-based, not polling). */
  if ( ( irq & LLCC68_IRQ_PREAMBLE_DETECTED ) != 0 )
  {
    printf( "[P2P][RX] PREAMBLE\r\n" );
    ( void )llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_PREAMBLE_DETECTED );
  }
  if ( ( irq & LLCC68_IRQ_HEADER_VALID ) != 0 )
  {
    printf( "[P2P][RX] HEADER VALID\r\n" );
    ( void )llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_HEADER_VALID );
  }
  if ( ( irq & LLCC68_IRQ_HEADER_ERROR ) != 0 )
  {
    printf( "[P2P][RX] HEADER ERROR\r\n" );
    ( void )llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_HEADER_ERROR );
  }
}
#endif /* LLCC68_P2P_ROLE */

/* Public functions ----------------------------------------------------------*/
bool LLCC68_P2P_Init(void)
{
  if ( llcc68_p2p_configure_radio( ) != true )
  {
    return false;
  }

#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
  printf( "[P2P][TX] INIT OK FREQ=%lu POWER=%d\r\n", ( unsigned long )LLCC68_P2P_FREQ_HZ,
          ( int )LLCC68_P2P_TX_PWR_DBM );
#else
  printf( "[P2P][RX] INIT OK FREQ=%lu\r\n", ( unsigned long )LLCC68_P2P_FREQ_HZ );
#endif

  llcc68_p2p_initialized = true;
  return true;
}

void LLCC68_P2P_Process(void)
{
  if ( llcc68_p2p_initialized != true )
  {
    return;
  }

#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
  llcc68_p2p_process_tx( );
#else
  llcc68_p2p_process_rx( );
#endif
}
