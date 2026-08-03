#ifndef LIGHT_H
#define LIGHT_H

#include "main.h"

HAL_StatusTypeDef Light_ReadLux(uint16_t *lux);
uint16_t Light_GetLed2Compare(uint16_t lux);

#endif
