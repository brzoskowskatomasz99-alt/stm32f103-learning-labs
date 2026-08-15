/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    llcc68_p2p.c
  * @brief   LLCC68 half-duplex protocol link for terminal and gateway roles.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "llcc68_p2p.h"
#include "main.h"
#include "llcc68_p2p_config.h"
#include "llcc68.h"
#include "llcc68_hal_stm32.h"
#include "terminal_sensors.h"
#include "service_control.h"
#include "service_hal.h"

#include <stdio.h>
#include <string.h>

typedef enum
{
  LLCC68_P2P_STATE_RX,
  LLCC68_P2P_STATE_WAIT_TX_DONE
} llcc68_p2p_state_t;

#define LLCC68_P2P_TX_DONE_TIMEOUT_MS  5000U

#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
#define LLCC68_P2P_ROLE_STR "TERMINAL"
#else
#define LLCC68_P2P_ROLE_STR "GATEWAY"
#endif

static bool llcc68_p2p_initialized = false;
static bool llcc68_p2p_rx_started = false;
static bool llcc68_p2p_tx_pending = false;
static bool llcc68_p2p_frame_received = false;
static llcc68_p2p_state_t llcc68_p2p_state = LLCC68_P2P_STATE_RX;
static uint32_t llcc68_p2p_state_tick = 0U;
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
static uint32_t llcc68_p2p_telemetry_tick = 0U;
static uint8_t llcc68_p2p_sequence = 1U;
static bool llcc68_p2p_has_last_command = false;
static uint16_t llcc68_p2p_last_command_id = 0U;
#endif
static uint8_t llcc68_p2p_tx_buffer[PROTOCOL_LORA_MAX_FRAME_SIZE];
static uint8_t llcc68_p2p_tx_length = 0U;
static uint8_t llcc68_p2p_rx_buffer[PROTOCOL_LORA_MAX_FRAME_SIZE];
static ProtocolLoraFrame llcc68_p2p_received_frame;
static LLCC68P2PRxMeta llcc68_p2p_received_meta;

#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
static bool llcc68_p2p_handle_terminal_command(const ProtocolLoraFrame *received)
{
  ProtocolLoraCommand command;
  ProtocolLoraAck ack;
  ProtocolLoraFrame ack_frame;

  if ( ( received->type != PROTOCOL_LORA_FRAME_COMMAND ) ||
       ( received->source_id != PROTOCOL_LORA_GATEWAY_ID ) ||
       ( received->destination_id != PROTOCOL_LORA_FIRST_TERMINAL_ID ) ||
       ( ProtocolLora_GetCommandPayload( received, &command ) != PROTOCOL_LORA_OK ) )
  {
    return false;
  }

  memset( &ack, 0, sizeof( ack ) );
  ack.command_id = command.command_id;
  ack.result = PROTOCOL_LORA_ACK_INVALID;
  if ( ( command.mode == PROTOCOL_LORA_MODE_MANUAL ) &&
       ( command.value <= 100U ) )
  {
    ServiceActuator actuator;
    bool known_actuator = true;

    switch ( command.actuator )
    {
      case PROTOCOL_LORA_ACTUATOR_LED:
        actuator = SERVICE_ACT_LED_STATUS;
        break;
      case PROTOCOL_LORA_ACTUATOR_BUZZER:
        actuator = SERVICE_ACT_BUZZER;
        break;
      case PROTOCOL_LORA_ACTUATOR_RELAY:
        actuator = SERVICE_ACT_RELAY;
        break;
      case PROTOCOL_LORA_ACTUATOR_LIGHT_PWM:
        actuator = SERVICE_ACT_LIGHT_PWM;
        break;
      case PROTOCOL_LORA_ACTUATOR_FAN_PWM:
        actuator = SERVICE_ACT_FAN_PWM;
        break;
      default:
        known_actuator = false;
        break;
    }

    if ( known_actuator == true )
    {
      uint8_t control_value = ( command.action == PROTOCOL_LORA_ACTION_OFF )
                                  ? 0U
                                  : ( uint8_t )command.value;

      /* LED 为二态：0 关、1..100 点亮（ACK 实际值 100） */
      if ( command.actuator == PROTOCOL_LORA_ACTUATOR_LED )
      {
        control_value = ( ( command.action == PROTOCOL_LORA_ACTION_OFF ) ||
                          ( command.value == 0U ) )
                            ? 0U
                            : 100U;
      }

      if ( ( llcc68_p2p_has_last_command != true ) ||
           ( llcc68_p2p_last_command_id != command.command_id ) )
      {
        if ( ServiceControl_ApplyManualCommand( actuator, control_value,
                                                &ack.actual_value ) )
        {
          ack.result = PROTOCOL_LORA_ACK_OK;
          llcc68_p2p_last_command_id = command.command_id;
          llcc68_p2p_has_last_command = true;
        }
      }
      else
      {
        /* 重复命令不重复执行（任务书可靠性要求） */
        ack.result = PROTOCOL_LORA_ACK_OK;
        ack.actual_value = ServiceControl_GetValue( actuator );
        printf( "[CONTROL][TERMINAL] DUP ID=%u\r\n",
                ( unsigned int )command.command_id );
      }
    }
  }

  memset( &ack_frame, 0, sizeof( ack_frame ) );
  ack_frame.version = PROTOCOL_LORA_VERSION_1;
  ack_frame.type = PROTOCOL_LORA_FRAME_ACK;
  ack_frame.source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID;
  ack_frame.destination_id = PROTOCOL_LORA_GATEWAY_ID;
  ack_frame.sequence = received->sequence;
  if ( ProtocolLora_SetAckPayload( &ack_frame, &ack ) != PROTOCOL_LORA_OK )
  {
    return true;
  }
  if ( LLCC68_P2P_QueueFrame( &ack_frame ) )
  {
    printf( "[CONTROL][TERMINAL] ACK ID=%u RESULT=%u ACTUAL=%u\r\n",
            ( unsigned int )ack.command_id, ( unsigned int )ack.result,
            ( unsigned int )ack.actual_value );
  }
  return true;
}
#endif

static const llcc68_pkt_params_lora_t llcc68_p2p_pkt_params = {
  LLCC68_P2P_PREAMBLE_SYMB,
  LLCC68_P2P_HEADER_TYPE,
  LLCC68_P2P_MAX_PAYLOAD_LEN,
  LLCC68_P2P_CRC_IS_ON,
  LLCC68_P2P_INVERT_IQ_IS_ON
};

static bool llcc68_p2p_start_rx(void)
{
  if ( llcc68_set_lora_pkt_params( &llcc68_hal_stm32_context, &llcc68_p2p_pkt_params ) !=
       LLCC68_STATUS_OK )
  {
    return false;
  }
  if ( llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_ALL ) != LLCC68_STATUS_OK )
  {
    return false;
  }
  if ( llcc68_set_rx_with_timeout_in_rtc_step( &llcc68_hal_stm32_context,
                                                LLCC68_RX_CONTINUOUS ) != LLCC68_STATUS_OK )
  {
    return false;
  }
  llcc68_p2p_rx_started = true;
  llcc68_p2p_state = LLCC68_P2P_STATE_RX;
  return true;
}

static bool llcc68_p2p_start_tx(void)
{
  llcc68_pkt_params_lora_t pkt_params = llcc68_p2p_pkt_params;

  if ( ( llcc68_p2p_tx_pending != true ) || ( llcc68_p2p_tx_length == 0U ) )
  {
    return false;
  }

  pkt_params.pld_len_in_bytes = llcc68_p2p_tx_length;
  if ( ( llcc68_set_standby( &llcc68_hal_stm32_context, LLCC68_STANDBY_CFG_RC ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_lora_pkt_params( &llcc68_hal_stm32_context, &pkt_params ) != LLCC68_STATUS_OK ) ||
       ( llcc68_write_buffer( &llcc68_hal_stm32_context, 0U, llcc68_p2p_tx_buffer,
                              llcc68_p2p_tx_length ) != LLCC68_STATUS_OK ) ||
       ( llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_ALL ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_tx( &llcc68_hal_stm32_context, 0U ) != LLCC68_STATUS_OK ) )
  {
    printf( "[P2P][%s] TX START FAIL\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }

  llcc68_p2p_rx_started = false;
  llcc68_p2p_state_tick = HAL_GetTick( );
  llcc68_p2p_state = LLCC68_P2P_STATE_WAIT_TX_DONE;
  return true;
}

static void llcc68_p2p_receive_packet(void)
{
  llcc68_rx_buffer_status_t rx_status;
  llcc68_pkt_status_lora_t pkt_status;
  ProtocolLoraStatus protocol_status;
  uint8_t payload_len;

  if ( llcc68_get_rx_buffer_status( &llcc68_hal_stm32_context, &rx_status ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] GET_RX_STATUS FAIL\r\n", LLCC68_P2P_ROLE_STR );
    return;
  }

  payload_len = rx_status.pld_len_in_bytes;
  if ( ( payload_len < ( PROTOCOL_LORA_HEADER_SIZE + PROTOCOL_LORA_CRC_SIZE ) ) ||
       ( payload_len > PROTOCOL_LORA_MAX_FRAME_SIZE ) )
  {
    printf( "[P2P][%s] RX LENGTH INVALID=%u\r\n", LLCC68_P2P_ROLE_STR,
            ( unsigned int )payload_len );
    return;
  }
  if ( llcc68_read_buffer( &llcc68_hal_stm32_context, rx_status.buffer_start_pointer,
                           llcc68_p2p_rx_buffer, payload_len ) != LLCC68_STATUS_OK )
  {
    printf( "[P2P][%s] READ_BUFFER FAIL\r\n", LLCC68_P2P_ROLE_STR );
    return;
  }

  protocol_status = ProtocolLora_Decode( llcc68_p2p_rx_buffer, payload_len,
                                        &llcc68_p2p_received_frame );
  if ( protocol_status != PROTOCOL_LORA_OK )
  {
    printf( "[P2P][%s] PROTOCOL DROP=%d\r\n", LLCC68_P2P_ROLE_STR,
            ( int )protocol_status );
    return;
  }

  llcc68_p2p_received_meta.valid = false;
  llcc68_p2p_received_meta.rssi_dbm = 0;
  llcc68_p2p_received_meta.snr_db = 0;
  printf( "[P2P][%s] RECV TYPE=%u SEQ=%u SRC=%04X DST=%04X LEN=%u\r\n",
          LLCC68_P2P_ROLE_STR, ( unsigned int )llcc68_p2p_received_frame.type,
          ( unsigned int )llcc68_p2p_received_frame.sequence,
          ( unsigned int )llcc68_p2p_received_frame.source_id,
          ( unsigned int )llcc68_p2p_received_frame.destination_id,
          ( unsigned int )llcc68_p2p_received_frame.payload_length );
  if ( llcc68_get_lora_pkt_status( &llcc68_hal_stm32_context, &pkt_status ) == LLCC68_STATUS_OK )
  {
    llcc68_p2p_received_meta.valid = true;
    llcc68_p2p_received_meta.rssi_dbm = pkt_status.rssi_pkt_in_dbm;
    llcc68_p2p_received_meta.snr_db = pkt_status.snr_pkt_in_db;
    printf( "[P2P][%s] RSSI=%d dBm SNR=%d dB\r\n", LLCC68_P2P_ROLE_STR,
            ( int )pkt_status.rssi_pkt_in_dbm, ( int )pkt_status.snr_pkt_in_db );
  }
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
  if ( llcc68_p2p_handle_terminal_command( &llcc68_p2p_received_frame ) )
  {
    return;
  }
#endif
  llcc68_p2p_frame_received = true;
}

#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
static void llcc68_p2p_queue_telemetry(void)
{
  ProtocolLoraFrame frame;
  ProtocolLoraTelemetry telemetry;
  TerminalSensorSnapshot sensors;

  memset( &frame, 0, sizeof( frame ) );
  memset( &telemetry, 0, sizeof( telemetry ) );
  memset( &sensors, 0, sizeof( sensors ) );
  frame.version = PROTOCOL_LORA_VERSION_1;
  frame.type = PROTOCOL_LORA_FRAME_TELEMETRY;
  frame.source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID;
  frame.destination_id = PROTOCOL_LORA_GATEWAY_ID;
  frame.sequence = llcc68_p2p_sequence++;

  if ( TerminalSensors_GetSnapshot( &sensors ) )
  {
    telemetry.temperature_x10 = sensors.temperature_x10;
    telemetry.humidity_x10 = sensors.humidity_x10;
    telemetry.co2_ppm = sensors.co2_ppm;
    telemetry.lux = sensors.lux;
    telemetry.soil_x10 = sensors.soil_x10;
    telemetry.device_status = sensors.device_status;
  }
  else
  {
    telemetry.device_status = TERMINAL_SENSOR_STATUS_DHT_INVALID |
                              TERMINAL_SENSOR_STATUS_CO2_INVALID |
                              TERMINAL_SENSOR_STATUS_LIGHT_INVALID |
                              TERMINAL_SENSOR_STATUS_SOIL_INVALID |
                              TERMINAL_SENSOR_STATUS_ADC_FAULT;
  }

  printf( "[SENSOR] T=%d H=%u CO2=%u LUX=%u SOIL=%u STATUS=%04X\r\n",
          ( int )telemetry.temperature_x10,
          ( unsigned int )telemetry.humidity_x10,
          ( unsigned int )telemetry.co2_ppm,
          ( unsigned int )telemetry.lux,
          ( unsigned int )telemetry.soil_x10,
          ( unsigned int )telemetry.device_status );
  if ( ProtocolLora_SetTelemetryPayload( &frame, &telemetry ) == PROTOCOL_LORA_OK )
  {
    ( void )LLCC68_P2P_QueueFrame( &frame );
  }
}
#endif

static bool llcc68_p2p_configure_radio(void)
{
  llcc68_chip_status_t chip_status;
  const llcc68_pa_cfg_params_t pa_cfg = { 0x04U, 0x07U, 0x00U, 0x01U };
  const llcc68_mod_params_lora_t mod_params = {
    LLCC68_P2P_LORA_SF, LLCC68_P2P_LORA_BW, LLCC68_P2P_LORA_CR, 0U
  };
  const llcc68_irq_mask_t irq_mask =
    LLCC68_IRQ_TX_DONE | LLCC68_IRQ_RX_DONE | LLCC68_IRQ_CRC_ERROR |
    LLCC68_IRQ_TIMEOUT | LLCC68_IRQ_HEADER_ERROR;

  if ( ( llcc68_reset( &llcc68_hal_stm32_context ) != LLCC68_STATUS_OK ) ||
       ( llcc68_get_status( &llcc68_hal_stm32_context, &chip_status ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_standby( &llcc68_hal_stm32_context, LLCC68_STANDBY_CFG_RC ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_dio2_as_rf_sw_ctrl( &llcc68_hal_stm32_context, true ) != LLCC68_STATUS_OK ) ||
       ( llcc68_cal_img_in_mhz( &llcc68_hal_stm32_context, LLCC68_P2P_CAL_IMG_FREQ1_MHZ,
                                LLCC68_P2P_CAL_IMG_FREQ2_MHZ ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_pkt_type( &llcc68_hal_stm32_context, LLCC68_PKT_TYPE_LORA ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_rf_freq( &llcc68_hal_stm32_context, LLCC68_P2P_FREQ_HZ ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_lora_mod_params( &llcc68_hal_stm32_context, &mod_params ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_lora_pkt_params( &llcc68_hal_stm32_context, &llcc68_p2p_pkt_params ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_buffer_base_address( &llcc68_hal_stm32_context, 0U, 0U ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_pa_cfg( &llcc68_hal_stm32_context, &pa_cfg ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_tx_params( &llcc68_hal_stm32_context, ( int8_t )LLCC68_P2P_TX_PWR_DBM,
                               LLCC68_RAMP_40_US ) != LLCC68_STATUS_OK ) ||
       ( llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_ALL ) != LLCC68_STATUS_OK ) ||
       ( llcc68_set_dio_irq_params( &llcc68_hal_stm32_context, irq_mask, 0U, 0U, 0U ) !=
         LLCC68_STATUS_OK ) )
  {
    printf( "[P2P][%s] INIT FAIL\r\n", LLCC68_P2P_ROLE_STR );
    return false;
  }
  return true;
}

bool LLCC68_P2P_Init(void)
{
  if ( llcc68_p2p_configure_radio( ) != true )
  {
    return false;
  }
  llcc68_p2p_initialized = true;
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
  llcc68_p2p_telemetry_tick = HAL_GetTick( );
#endif
  printf( "[P2P][%s] INIT OK FREQ=%lu SF=9 POWER=%d\r\n", LLCC68_P2P_ROLE_STR,
          ( unsigned long )LLCC68_P2P_FREQ_HZ, ( int )LLCC68_P2P_TX_PWR_DBM );
  return true;
}

bool LLCC68_P2P_QueueFrame(const ProtocolLoraFrame *frame)
{
  size_t encoded_length = 0U;

  if ( ( frame == NULL ) || ( llcc68_p2p_tx_pending == true ) )
  {
    return false;
  }
  if ( ProtocolLora_Encode( frame, llcc68_p2p_tx_buffer,
                            sizeof( llcc68_p2p_tx_buffer ), &encoded_length ) != PROTOCOL_LORA_OK )
  {
    return false;
  }
  llcc68_p2p_tx_length = ( uint8_t )encoded_length;
  llcc68_p2p_tx_pending = true;
  return true;
}

bool LLCC68_P2P_IsTxPending(void)
{
  return llcc68_p2p_tx_pending;
}

bool LLCC68_P2P_TakeReceivedFrameWithMeta(ProtocolLoraFrame *frame,
                                          LLCC68P2PRxMeta *meta)
{
  if ( ( frame == NULL ) || ( meta == NULL ) ||
       ( llcc68_p2p_frame_received != true ) )
  {
    return false;
  }
  *frame = llcc68_p2p_received_frame;
  *meta = llcc68_p2p_received_meta;
  llcc68_p2p_frame_received = false;
  return true;
}

bool LLCC68_P2P_TakeReceivedFrame(ProtocolLoraFrame *frame)
{
  LLCC68P2PRxMeta meta;

  return LLCC68_P2P_TakeReceivedFrameWithMeta( frame, &meta );
}

void LLCC68_P2P_Process(void)
{
  llcc68_irq_mask_t irq = LLCC68_IRQ_NONE;

  if ( llcc68_p2p_initialized != true )
  {
    return;
  }

  if ( llcc68_p2p_state == LLCC68_P2P_STATE_WAIT_TX_DONE )
  {
    if ( llcc68_get_irq_status( &llcc68_hal_stm32_context, &irq ) != LLCC68_STATUS_OK )
    {
      printf( "[P2P][%s] GET IRQ FAIL\r\n", LLCC68_P2P_ROLE_STR );
      llcc68_p2p_state = LLCC68_P2P_STATE_RX;
    }
    else if ( ( irq & LLCC68_IRQ_TX_DONE ) != 0U )
    {
      printf( "[P2P][%s] SENT LEN=%u\r\n", LLCC68_P2P_ROLE_STR,
              ( unsigned int )llcc68_p2p_tx_length );
      llcc68_p2p_tx_pending = false;
      llcc68_p2p_tx_length = 0U;
      llcc68_p2p_state = LLCC68_P2P_STATE_RX;
    }
    else if ( ( ( irq & LLCC68_IRQ_TIMEOUT ) != 0U ) ||
              ( ( HAL_GetTick( ) - llcc68_p2p_state_tick ) >= LLCC68_P2P_TX_DONE_TIMEOUT_MS ) )
    {
      printf( "[P2P][%s] TX TIMEOUT\r\n", LLCC68_P2P_ROLE_STR );
      llcc68_p2p_tx_pending = false;
      llcc68_p2p_tx_length = 0U;
      llcc68_p2p_state = LLCC68_P2P_STATE_RX;
    }
    if ( llcc68_p2p_state == LLCC68_P2P_STATE_RX )
    {
      llcc68_p2p_rx_started = false;
    }
    return;
  }

#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
  if ( ( llcc68_p2p_tx_pending != true ) &&
       ( ( HAL_GetTick( ) - llcc68_p2p_telemetry_tick ) >= LLCC68_P2P_TX_INTERVAL_MS ) )
  {
    llcc68_p2p_telemetry_tick = HAL_GetTick( );
    llcc68_p2p_queue_telemetry( );
  }
#endif

  if ( llcc68_p2p_tx_pending == true )
  {
    ( void )llcc68_p2p_start_tx( );
    return;
  }
  if ( llcc68_p2p_rx_started != true )
  {
    if ( llcc68_p2p_start_rx( ) != true )
    {
      printf( "[P2P][%s] RX START FAIL\r\n", LLCC68_P2P_ROLE_STR );
    }
    return;
  }
  if ( llcc68_get_irq_status( &llcc68_hal_stm32_context, &irq ) != LLCC68_STATUS_OK )
  {
    return;
  }
  if ( ( irq & LLCC68_IRQ_RX_DONE ) != 0U )
  {
    llcc68_p2p_receive_packet( );
    ( void )llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_ALL );
  }
  else if ( ( irq & ( LLCC68_IRQ_CRC_ERROR | LLCC68_IRQ_HEADER_ERROR ) ) != 0U )
  {
    printf( "[P2P][%s] RADIO DROP IRQ=%04X\r\n", LLCC68_P2P_ROLE_STR, ( unsigned int )irq );
    ( void )llcc68_clear_irq_status( &llcc68_hal_stm32_context, LLCC68_IRQ_ALL );
  }
}
