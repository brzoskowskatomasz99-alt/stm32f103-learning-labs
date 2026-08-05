#ifndef CO2_H
#define CO2_H

#include "main.h"
#include "usart.h"

#define CO2_RX_BUF_SIZE 6

extern uint8_t co2_buffer[CO2_RX_BUF_SIZE];
extern volatile uint8_t co2_rx_len;

void CO2_UART_Callback(uint16_t Size);
void CO2_UART_Receive_Start(void);
uint8_t CO2_get_data(uint16_t *co2_value, uint32_t timeout);

#endif









