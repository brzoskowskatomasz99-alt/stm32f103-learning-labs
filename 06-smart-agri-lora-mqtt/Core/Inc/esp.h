#ifndef ESP_H
#define ESP_H

#include "usart.h"
#include "main.h"
#include <stdint.h>

/* 凭据迁出：WIFI_SSID/WIFI_PASSWORD 等私有值在 secrets.h（不入库）。
   新 clone 请复制 secrets_template.h 为 secrets.h 并填入真实值。 */
#include "secrets.h"

#define ESP_UART_HANDLE &huart2
#define ESP_RX_BUF_SIZE 128

#define TCP_SERVER "bemfa.com"
#define TCP_SERVER_PORT "9501"

extern uint8_t ESP_buffer[ESP_RX_BUF_SIZE];
extern volatile int32_t ESP_rx_len;

void ESP_UART_Callback(uint16_t Size);
void ESP_UART_Receive_Start(void);

HAL_StatusTypeDef ESP_Send_data_len(const uint8_t *data, uint16_t len, uint16_t timeout);
int32_t ESP_Get_Receive_Data(uint8_t *recv_buf, int32_t *recv_len, uint32_t timeout);
int8_t ESP_Send_AT_Cmd(const char *cmd, const char *wait_string, uint32_t timeout);
int8_t ESP_Exit_Transmit_Mode(void);
int8_t ESP_Connect_WiFi(char *ssid, char *password);
int8_t ESP_Connect_Server(char *ip, char *port);
int8_t ESP_Init(void);

#endif
