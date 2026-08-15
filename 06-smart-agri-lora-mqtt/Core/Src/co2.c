#include "co2.h"

#include <string.h>

uint8_t co2_buffer[CO2_RX_BUF_SIZE] = {0U};
volatile uint8_t co2_rx_len = 0U;

void CO2_UART_Callback(uint16_t Size)
{
    if (Size > CO2_RX_BUF_SIZE)
    {
        Size = CO2_RX_BUF_SIZE;
    }

    /* Keep the completed frame stable until the main loop consumes it. */
    co2_rx_len = (uint8_t)Size;
}

void CO2_UART_Receive_Start(void)
{
    co2_rx_len = 0U;
    memset(co2_buffer, 0, sizeof(co2_buffer));

    (void)HAL_UARTEx_ReceiveToIdle_IT(&huart2,
                                      co2_buffer,
                                      sizeof(co2_buffer));
}

uint8_t CO2_TakeLatest(uint16_t *co2_value)
{
    uint8_t frame[CO2_RX_BUF_SIZE];
    uint8_t check_sum = 0U;
    uint8_t i;

    if ((co2_value == NULL) || (co2_rx_len == 0U))
    {
        return CO2_STATUS_NO_DATA;
    }

    if (co2_rx_len != CO2_RX_BUF_SIZE)
    {
        CO2_UART_Receive_Start();
        return CO2_STATUS_BAD_HEADER;
    }

    memcpy(frame, co2_buffer, sizeof(frame));
    CO2_UART_Receive_Start();

    if (frame[0] != 0x2CU)
    {
        return CO2_STATUS_BAD_HEADER;
    }

    for (i = 0U; i < (CO2_RX_BUF_SIZE - 1U); i++)
    {
        check_sum = (uint8_t)(check_sum + frame[i]);
    }

    if (check_sum != frame[CO2_RX_BUF_SIZE - 1U])
    {
        return CO2_STATUS_BAD_CHECKSUM;
    }

    *co2_value = (uint16_t)(((uint16_t)frame[1] << 8) | frame[2]);
    return CO2_STATUS_OK;
}

uint8_t CO2_get_data(uint16_t *co2_value, uint32_t timeout)
{
    uint32_t start_time = HAL_GetTick();
    uint8_t status;

    while ((HAL_GetTick() - start_time) <= timeout)
    {
        status = CO2_TakeLatest(co2_value);
        if (status != CO2_STATUS_NO_DATA)
        {
            return status;
        }
    }

    return CO2_STATUS_NO_DATA;
}
