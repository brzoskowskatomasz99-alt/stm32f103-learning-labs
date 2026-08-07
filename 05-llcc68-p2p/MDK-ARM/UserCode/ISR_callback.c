#include "ISR_callback.h"
#include "app_main.h"
#include "main.h"
#include "co2.h"


volatile uint8_t key_pressed = 0;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == KEY1_Pin)
    {
        key_pressed = 1;
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
    }

}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        App_CommandRxEvent(Size);
    }
    else if (huart->Instance == USART2)
    {
        CO2_UART_Callback(Size);
    }
}
