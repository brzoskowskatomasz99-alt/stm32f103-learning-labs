#ifndef CO2_H
#define CO2_H

#include "main.h"
#include "usart.h"

#define CO2_RX_BUF_SIZE 6U

#define CO2_STATUS_OK            0U
#define CO2_STATUS_NO_DATA       1U
#define CO2_STATUS_BAD_HEADER    2U
#define CO2_STATUS_BAD_CHECKSUM  3U

extern uint8_t co2_buffer[CO2_RX_BUF_SIZE];
extern volatile uint8_t co2_rx_len;

void CO2_UART_Callback(uint16_t Size);
void CO2_UART_Receive_Start(void);
uint8_t CO2_TakeLatest(uint16_t *co2_value);
uint8_t CO2_get_data(uint16_t *co2_value, uint32_t timeout);

#endif
