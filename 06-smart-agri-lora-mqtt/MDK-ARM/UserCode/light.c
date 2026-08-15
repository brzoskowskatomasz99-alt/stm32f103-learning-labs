#include "light.h"
#include "adc.h"
#include "tim.h"

typedef struct
{
    uint32_t resistance_ohm;
    uint16_t lux;
} LightLookupEntry;

static const LightLookupEntry gl5528_lookup[] =
{
    {100000U, 0U},
    {70000U,  1U},
    {50000U,  1U},
    {40000U,  1U},
    {30000U,  2U},
    {20000U,  4U},
    {15000U,  5U},
    {10000U, 10U},
    {7000U,  17U},
    {5000U,  29U},
    {4000U,  45U},
    {3000U,  68U},
    {2000U, 124U},
    {1000U, 350U}
};

HAL_StatusTypeDef Light_ReadLux(uint16_t *lux)
{
    uint16_t adc_value;
    uint32_t resistance_ohm;
    uint32_t i;

    if (lux == NULL)
    {
        return HAL_ERROR;
    }

    adc_value = adc1_values[0];

    if (adc_value >= 4095U)
    {
        *lux = 0U;
        return HAL_OK;
    }

    resistance_ohm = (10000U * (uint32_t)adc_value) /
                     (4095U - (uint32_t)adc_value);

    *lux = gl5528_lookup[(sizeof(gl5528_lookup) / sizeof(gl5528_lookup[0])) - 1U].lux;
    for (i = 0U; i < (sizeof(gl5528_lookup) / sizeof(gl5528_lookup[0])); i++)
    {
        if (resistance_ohm >= gl5528_lookup[i].resistance_ohm)
        {
            *lux = gl5528_lookup[i].lux;
            break;
        }
    }

    return HAL_OK;
}

uint16_t Light_GetLed2Compare(uint16_t lux)
{
    if (lux >= 20U)
    {
        return 0U;
    }

    return (uint16_t)(((20U - lux) * 999U) / 20U);
}

/* Legacy gateway local LED output on TIM1_CH1/PA8.
   The terminal build does not initialize TIM1; terminal LED2 is PB15 and is
   intentionally left for the later actuator-control phase. */
void Light_Led2_Init(void)
{
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    Light_Led2_Off();
}

void Light_Led2_SetBrightness(uint8_t percent)
{
    uint32_t compare;

    if (percent > 100U)
    {
        percent = 100U;
    }
    if (percent == 0U)
    {
        Light_Led2_Off();
        return;
    }

    compare = ((100U - percent) * LIGHT_LED2_PWM_PERIOD) / 100U;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare);
}

void Light_Led2_Off(void)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, LIGHT_LED2_PWM_PERIOD);
}
