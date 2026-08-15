#ifndef SOIL_H
#define SOIL_H

#include "main.h"

#define SOIL_RESISTANCE_OPEN_CIRCUIT UINT32_MAX

HAL_StatusTypeDef Soil_ReadHumidityLevel(uint8_t *level,
                                         uint32_t *resistance_ohm);
HAL_StatusTypeDef Soil_ReadHumidityX10(uint16_t *humidity_x10);

#endif
