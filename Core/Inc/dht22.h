#ifndef __DHT22_H
#define __DHT22_H

#include "stm32f1xx_hal.h"

#define DHT22_GPIO_PORT GPIOC
#define DHT22_GPIO_PIN  GPIO_PIN_15

void DHT22_Init(void);
uint8_t DHT22_ReadData(float *temp, float *humi);

#endif
