#include "dht22.h"

static void DHT22_SetOutputMode(void);
static void DHT22_SetInputMode(void);
static uint8_t DHT22_SendStartSignal(void);
static uint8_t DHT22_ReadByte(void);
static void DHT22_DelayUs(uint32_t us);

void DHT22_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    DHT22_SetOutputMode();
    HAL_GPIO_WritePin(DHT22_GPIO_PORT, DHT22_GPIO_PIN, GPIO_PIN_SET);
}

uint8_t DHT22_ReadData(float *temp, float *humi)
{
    uint8_t data[5] = {0U};
    uint8_t index;
    uint8_t start_status;
    uint16_t raw_temp;
    uint16_t raw_humi;

    start_status = DHT22_SendStartSignal();
    if (start_status != 0U)
    {
        return start_status;
    }

    for (index = 0U; index < 5U; index++)
    {
        data[index] = DHT22_ReadByte();
        if (data[index] == 0xFFU)
        {
            DHT22_SetOutputMode();
            HAL_GPIO_WritePin(DHT22_GPIO_PORT, DHT22_GPIO_PIN, GPIO_PIN_SET);
            return 2U;
        }
    }

    DHT22_SetOutputMode();
    HAL_GPIO_WritePin(DHT22_GPIO_PORT, DHT22_GPIO_PIN, GPIO_PIN_SET);

    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
    {
        return 3U;
    }

    raw_humi = ((uint16_t)data[0] << 8) | data[1];
    raw_temp = ((uint16_t)(data[2] & 0x7FU) << 8) | data[3];

    *humi = (float)raw_humi / 10.0F;
    *temp = (float)raw_temp / 10.0F;
    if ((data[2] & 0x80U) != 0U)
    {
        *temp = -*temp;
    }

    return 0U;
}

static void DHT22_SetOutputMode(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = DHT22_GPIO_PIN;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT22_GPIO_PORT, &gpio_init);
}

static void DHT22_SetInputMode(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = DHT22_GPIO_PIN;
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DHT22_GPIO_PORT, &gpio_init);
}

static uint8_t DHT22_SendStartSignal(void)
{
    uint32_t timeout;

    DHT22_SetOutputMode();
    HAL_GPIO_WritePin(DHT22_GPIO_PORT, DHT22_GPIO_PIN, GPIO_PIN_RESET);
    HAL_Delay(1U);
    HAL_GPIO_WritePin(DHT22_GPIO_PORT, DHT22_GPIO_PIN, GPIO_PIN_SET);
    DHT22_DelayUs(30U);
    DHT22_SetInputMode();

    timeout = 100U;
    while (HAL_GPIO_ReadPin(DHT22_GPIO_PORT, DHT22_GPIO_PIN) == GPIO_PIN_SET)
    {
        if (timeout-- == 0U)
        {
            return 1U;
        }
        DHT22_DelayUs(1U);
    }

    timeout = 100U;
    while (HAL_GPIO_ReadPin(DHT22_GPIO_PORT, DHT22_GPIO_PIN) == GPIO_PIN_RESET)
    {
        if (timeout-- == 0U)
        {
            return 4U;
        }
        DHT22_DelayUs(1U);
    }

    return 0U;
}

static uint8_t DHT22_ReadByte(void)
{
    uint8_t value = 0U;
    uint8_t bit_index;
    uint32_t timeout;

    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
        timeout = 100U;
        while (HAL_GPIO_ReadPin(DHT22_GPIO_PORT, DHT22_GPIO_PIN) == GPIO_PIN_SET)
        {
            if (timeout-- == 0U)
            {
                return 0xFFU;
            }
            DHT22_DelayUs(1U);
        }

        timeout = 100U;
        while (HAL_GPIO_ReadPin(DHT22_GPIO_PORT, DHT22_GPIO_PIN) == GPIO_PIN_RESET)
        {
            if (timeout-- == 0U)
            {
                return 0xFFU;
            }
            DHT22_DelayUs(1U);
        }

        DHT22_DelayUs(30U);
        value <<= 1;
        if (HAL_GPIO_ReadPin(DHT22_GPIO_PORT, DHT22_GPIO_PIN) == GPIO_PIN_SET)
        {
            value |= 1U;
        }
    }

    return value;
}

static void DHT22_DelayUs(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (HAL_RCC_GetHCLKFreq() / 1000000U);

    while ((DWT->CYCCNT - start) < ticks)
    {
    }
}
