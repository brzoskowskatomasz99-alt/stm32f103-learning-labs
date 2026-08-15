#ifndef LIGHT_H
#define LIGHT_H

#include "main.h"

HAL_StatusTypeDef Light_ReadLux(uint16_t *lux);
uint16_t Light_GetLed2Compare(uint16_t lux);

#define LIGHT_LED2_PWM_PERIOD 999U

void Light_Led2_Init(void);
void Light_Led2_SetBrightness(uint8_t percent);
void Light_Led2_Off(void);

#endif
