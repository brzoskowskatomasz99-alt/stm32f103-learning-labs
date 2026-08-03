#ifndef OLED_H
#define OLED_H

#include "stm32f1xx_hal.h"

HAL_StatusTypeDef OLED_Init(void);
HAL_StatusTypeDef OLED_TestPattern(void);
HAL_StatusTypeDef OLED_Clear(void);
HAL_StatusTypeDef OLED_ShowText(uint8_t x, uint8_t page, const char *text);
HAL_StatusTypeDef OLED_ShowTemperatureIcon(uint8_t x, uint8_t page);
void OLED_FrameClear(void);
void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color);
void OLED_FillRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                   uint8_t color);
void OLED_DrawBitmap(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                     const uint8_t *bitmap);
HAL_StatusTypeDef OLED_Refresh(void);

#endif
