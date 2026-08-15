#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "main.h"


// 函数声明
void delay_ms(uint32_t ms);
void app_main(void);
void delay_us(uint32_t nus);
void App_CommandReceptionStart(void);
void App_CommandRxEvent(uint16_t size);
#endif
