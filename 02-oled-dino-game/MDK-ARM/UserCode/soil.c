#include "soil.h"
#include "app_main.h"

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

    if (adc_value >= 4095U)
    {
        *resistance_ohm = SOIL_RESISTANCE_OPEN_CIRCUIT;
        *level = 1U;
        return HAL_OK;
    }

    *resistance_ohm = (10000U * (uint32_t)adc_value) /
                      (4095U - (uint32_t)adc_value);

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
