#include "co2.h"
#include <string.h>




uint8_t co2_buffer[CO2_RX_BUF_SIZE];

volatile uint8_t co2_rx_len;


void CO2_UART_Callback(uint16_t Size)
{
    co2_rx_len = Size;

    HAL_UARTEx_ReceiveToIdle_IT(&huart2,
                                co2_buffer,
                                sizeof(co2_buffer));
}


void CO2_UART_Receive_Start(void)
{
    co2_rx_len = 0;

    memset(co2_buffer,0,sizeof(co2_buffer));

    HAL_UARTEx_ReceiveToIdle_IT(&huart2,
                                co2_buffer,
                                sizeof(co2_buffer));
}


uint8_t CO2_get_data(uint16_t *co2_value, uint32_t timeout)
{
    uint32_t start_time = HAL_GetTick();

    while (co2_rx_len != CO2_RX_BUF_SIZE)
    {
        if (HAL_GetTick() - start_time > timeout)
        {
            return 1;
        }
    }

    co2_rx_len = 0;

    if (co2_buffer[0] != 0x2C)
    {
        return 2;
    }

    uint8_t check_sum = 0;
    for (uint8_t i = 0; i < CO2_RX_BUF_SIZE - 1; i++)
    {
        check_sum += co2_buffer[i];
    }

    if (check_sum != co2_buffer[CO2_RX_BUF_SIZE - 1])
    {
        return 3;
    }

    *co2_value = ((uint16_t)co2_buffer[1] << 8) | co2_buffer[2];

    return 0;
}
