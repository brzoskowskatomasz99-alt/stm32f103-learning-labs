#ifndef SERVICE_HAL_H
#define SERVICE_HAL_H

#include <stdint.h>

/*
 * 执行器硬件抽象层：业务模块（service_control/service_alarm）只依赖本接口，
 * 不直接调用 HAL/GPIO/PWM。固件侧实现见 service_hal.c，主机单测用桩实现。
 */

typedef enum
{
    SERVICE_ACT_LED_STATUS = 0, /* PB15 板载状态灯（板丝印 LED3，低电平点亮） */
    SERVICE_ACT_BUZZER,         /* PB9 蜂鸣器（高电平响） */
    SERVICE_ACT_RELAY,          /* PC14 灌溉继电器（高电平吸合） */
    SERVICE_ACT_LIGHT_PWM,      /* PB8/TIM4_CH3 灯光 PWM，value=0..100 */
    SERVICE_ACT_FAN_PWM,        /* PA8/TIM1_CH1 风机 PWM，value=0..100 */
    SERVICE_ACT_COUNT
} ServiceActuator;

void ServiceHal_Init(void);
uint32_t ServiceHal_GetTickMs(void);
void ServiceHal_ActuatorWrite(ServiceActuator actuator, uint8_t value);
uint8_t ServiceHal_ActuatorRead(ServiceActuator actuator);

#endif /* SERVICE_HAL_H */
