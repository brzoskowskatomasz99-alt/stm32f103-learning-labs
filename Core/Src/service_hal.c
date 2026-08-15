#include "service_hal.h"

#include "main.h"
#include "tim.h"

/*
 * 终端板执行器引脚（依据底板原理图 SCH_智慧农业终端_2026-03-29.pdf
 * 导线连通性追踪，2026-08-14 修正）：
 *   LED_STATUS: PB15（板丝印 LED3，低电平点亮）
 *   BUZZER:     PB9 （Q2 S8050 驱动，高电平响）
 *   RELAY:      PC14（Q4 S8050 驱动，高电平吸合）
 *   LIGHT_PWM:  PA8 = TIM1_CH1（LED2 灯光，低电平点亮 → 反相 PWM）
 *   FAN_PWM:    PB8 = TIM4_CH3（Q3 SI2302 风机，高电平导通）
 */
#define SERVICE_LED_PORT          GPIOB
#define SERVICE_LED_PIN           GPIO_PIN_15
#define SERVICE_BEEP_PORT         GPIOB
#define SERVICE_BEEP_PIN          GPIO_PIN_9
#define SERVICE_RELAY_PORT        GPIOC
#define SERVICE_RELAY_PIN         GPIO_PIN_14
#define SERVICE_LIGHT_PWM_PERIOD  999U  /* TIM1 ARR（低电平点亮：compare=999 灭） */
#define SERVICE_FAN_PWM_PERIOD    99U   /* TIM4 ARR（高电平导通：compare=0 停） */

static uint8_t service_actuator_state[SERVICE_ACT_COUNT];

void ServiceHal_Init(void)
{
    uint8_t index;

    for (index = 0U; index < SERVICE_ACT_COUNT; ++index)
    {
        service_actuator_state[index] = 0U;
    }

    /* 默认安全状态：灯灭、蜂鸣器停、继电器断、PWM 0% */
    HAL_GPIO_WritePin(SERVICE_LED_PORT, SERVICE_LED_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SERVICE_BEEP_PORT, SERVICE_BEEP_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SERVICE_RELAY_PORT, SERVICE_RELAY_PIN, GPIO_PIN_RESET);

    (void)HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
    (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    /* 灯光 LED2 低电平点亮：灭 = compare 满周期（常高） */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, SERVICE_LIGHT_PWM_PERIOD);
    /* 风机高电平导通：停 = compare 0（常低） */
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, 0U);
}

uint32_t ServiceHal_GetTickMs(void)
{
    return HAL_GetTick();
}

void ServiceHal_ActuatorWrite(ServiceActuator actuator, uint8_t value)
{
    if (actuator >= SERVICE_ACT_COUNT)
    {
        return;
    }

    service_actuator_state[actuator] = value;

    switch (actuator)
    {
    case SERVICE_ACT_LED_STATUS:
        HAL_GPIO_WritePin(SERVICE_LED_PORT, SERVICE_LED_PIN,
                          (value == 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;
    case SERVICE_ACT_BUZZER:
        HAL_GPIO_WritePin(SERVICE_BEEP_PORT, SERVICE_BEEP_PIN,
                          (value == 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        break;
    case SERVICE_ACT_RELAY:
        HAL_GPIO_WritePin(SERVICE_RELAY_PORT, SERVICE_RELAY_PIN,
                          (value == 0U) ? GPIO_PIN_RESET : GPIO_PIN_SET);
        break;
    case SERVICE_ACT_LIGHT_PWM:
        if (value > 100U)
        {
            value = 100U;
        }
        /* LED2 低电平点亮：value=100 → compare 0（常亮），value=0 → 999（灭） */
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1,
                              ((uint32_t)(100U - value) *
                               SERVICE_LIGHT_PWM_PERIOD) /
                                  100U);
        break;
    case SERVICE_ACT_FAN_PWM:
        if (value > 100U)
        {
            value = 100U;
        }
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3,
                              ((uint32_t)value * SERVICE_FAN_PWM_PERIOD) /
                                  100U);
        break;
    default:
        break;
    }
}

uint8_t ServiceHal_ActuatorRead(ServiceActuator actuator)
{
    if (actuator >= SERVICE_ACT_COUNT)
    {
        return 0U;
    }
    return service_actuator_state[actuator];
}
