#include "soil.h"
#include "adc.h"

typedef struct
{
    uint32_t resistance_ohm;
    uint8_t humidity_level;
} SoilLookupEntry;

static const SoilLookupEntry soil_lookup[] =
{
    {100000U, 1U},
    { 50000U, 2U},
    { 20000U, 3U},
    { 10000U, 4U}
};

static uint32_t Soil_AdcToResistance(uint16_t adc_value)
{
    if (adc_value >= 4095U)
    {
        return SOIL_RESISTANCE_OPEN_CIRCUIT;
    }

    return (10000U * (uint32_t)adc_value) /
           (4095U - (uint32_t)adc_value);
}

HAL_StatusTypeDef Soil_ReadHumidityLevel(uint8_t *level,
                                         uint32_t *resistance_ohm)
{
    uint16_t adc_value;
    uint32_t i;

    if ((level == NULL) || (resistance_ohm == NULL))
    {
        return HAL_ERROR;
    }

    adc_value = adc1_values[1];

    *resistance_ohm = Soil_AdcToResistance(adc_value);
    if (*resistance_ohm == SOIL_RESISTANCE_OPEN_CIRCUIT)
    {
        *level = 1U;
        return HAL_OK;
    }

    *level = soil_lookup[(sizeof(soil_lookup) / sizeof(soil_lookup[0])) - 1U].humidity_level;
    for (i = 0U; i < (sizeof(soil_lookup) / sizeof(soil_lookup[0])); i++)
    {
        if (*resistance_ohm >= soil_lookup[i].resistance_ohm)
        {
            *level = soil_lookup[i].humidity_level;
            break;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef Soil_ReadHumidityX10(uint16_t *humidity_x10)
{
    uint32_t resistance_ohm;

    if (humidity_x10 == NULL)
    {
        return HAL_ERROR;
    }

    resistance_ohm = Soil_AdcToResistance(adc1_values[1]);
    if (resistance_ohm == SOIL_RESISTANCE_OPEN_CIRCUIT)
    {
        *humidity_x10 = 0U;
        return HAL_ERROR;
    }

    /* Convert the existing four resistance bands to 0.0-100.0%.
       These anchors are an engineering default and still require field
       calibration with the actual probe and soil. */
    if (resistance_ohm >= 100000U)
    {
        *humidity_x10 = 0U;
    }
    else if (resistance_ohm >= 50000U)
    {
        *humidity_x10 = (uint16_t)(((100000U - resistance_ohm) * 333U) /
                                   50000U);
    }
    else if (resistance_ohm >= 20000U)
    {
        *humidity_x10 = (uint16_t)(333U +
                                   (((50000U - resistance_ohm) * 334U) /
                                    30000U));
    }
    else if (resistance_ohm >= 10000U)
    {
        *humidity_x10 = (uint16_t)(667U +
                                   (((20000U - resistance_ohm) * 333U) /
                                    10000U));
    }
    else
    {
        *humidity_x10 = 1000U;
    }

    return HAL_OK;
}
