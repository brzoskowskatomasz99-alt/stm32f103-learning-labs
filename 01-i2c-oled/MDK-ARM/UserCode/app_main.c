#if 0
/* 原环境监测业务逻辑：本 I2C/OLED 实验暂不使用，保留以便后续恢复。 */
#include "app_main.h"
#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "co2.h"
#include "dht22.h"
#include "light.h"
#include "soil.h"
#include "oled.h"

#define TEMP_ALARM_ON_C           28.0F
#define TEMP_ALARM_OFF_C          27.7F
#define HUMIDITY_ALARM_PERCENT    70.0F
#define CO2_LIGHT_MIN_PPM         400U
#define CO2_LIGHT_MAX_PPM         2000U
#define LIGHT_ALARM_LUX           20U
#define SOIL_ALARM_LEVEL          2U
#define LIGHT_PWM_MIN_PERCENT     10U
#define LAMP_PWM_PERIOD           99U
#define LED2_PWM_PERIOD           999U
#define COMMAND_RX_BUFFER_SIZE    64U

volatile uint16_t adc1_values[2];

static uint8_t command_rx_buffer[COMMAND_RX_BUFFER_SIZE];
static volatile uint16_t command_rx_size;
static volatile uint8_t command_rx_ready;
static uint8_t automatic_control = 1U;
static uint8_t manual_lamp_percent = 100U;

static float latest_temperature;
static float latest_humidity;
static uint16_t latest_co2;
static uint16_t latest_lux;
static uint8_t latest_soil_level;
static uint8_t temperature_valid;
static uint8_t humidity_valid;
static uint8_t co2_valid;
static uint8_t lux_valid;
static uint8_t soil_valid;
static uint8_t temperature_alarm;

static uint32_t CO2_ToLightPercent(uint16_t co2_ppm)
{
    if (co2_ppm <= CO2_LIGHT_MIN_PPM)
    {
        return LIGHT_PWM_MIN_PERCENT;
    }
    if (co2_ppm >= CO2_LIGHT_MAX_PPM)
    {
        return 100U;
    }

    return LIGHT_PWM_MIN_PERCENT +
           ((uint32_t)(co2_ppm - CO2_LIGHT_MIN_PPM) *
            (100U - LIGHT_PWM_MIN_PERCENT)) /
           (CO2_LIGHT_MAX_PPM - CO2_LIGHT_MIN_PPM);
}

static void App_SetLampPercent(uint8_t percent)
{
    if (percent > 100U)
    {
        percent = 100U;
    }

    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3,
                          ((uint32_t)percent * LAMP_PWM_PERIOD) / 100U);
}

static void App_SetLed2(uint8_t on)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1,
                          on ? 0U : LED2_PWM_PERIOD);
}

static uint8_t App_IsAlarmActive(void)
{
    return (uint8_t)(temperature_alarm ||
                     (humidity_valid && (latest_humidity > HUMIDITY_ALARM_PERCENT)) ||
                     (co2_valid && (latest_co2 >= CO2_LIGHT_MAX_PPM)) ||
                     (lux_valid && (latest_lux < LIGHT_ALARM_LUX)) ||
                     (soil_valid && (latest_soil_level == SOIL_ALARM_LEVEL)));
}

static void App_ApplyAutomaticControl(void)
{
    if (automatic_control == 0U)
    {
        return;
    }

    if (lux_valid != 0U)
    {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1,
                              Light_GetLed2Compare(latest_lux));
    }
    if (co2_valid != 0U)
    {
        App_SetLampPercent((uint8_t)CO2_ToLightPercent(latest_co2));
    }
    if (soil_valid != 0U)
    {
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin,
                          (latest_soil_level == SOIL_ALARM_LEVEL) ?
                          GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
}

static void App_PrintAllSensors(void)
{
    int32_t temperature_tenths = (int32_t)(latest_temperature * 10.0F);
    uint32_t humidity_tenths = (uint32_t)(latest_humidity * 10.0F);

    printf("\r\n--- 当前传感器数据 ---\r\n");
    printf("温度：%ld.%ld ℃\r\n",
           (long)(temperature_tenths / 10),
           (long)((temperature_tenths < 0 ? -temperature_tenths : temperature_tenths) % 10));
    printf("空气湿度：%lu.%lu %%\r\n",
           (unsigned long)(humidity_tenths / 10U),
           (unsigned long)(humidity_tenths % 10U));
    printf("CO2 浓度：%u ppm\r\n", (unsigned int)latest_co2);
    printf("光照强度：%lu lux\r\n", (unsigned long)latest_lux);
    printf("土壤湿度等级：%u\r\n", (unsigned int)latest_soil_level);
    printf("控制模式：%s\r\n",
           automatic_control ? "自动" : "手动");
}

static void App_PrintHelp(void)
{
    printf("查询：GET ALL / GET TEMP / GET HUM / GET CO2 / GET LUX / GET SOIL\r\n");
    printf("控制：SET LAMP ON|OFF，SET LED2 ON|OFF，SET RELAY ON|OFF\r\n");
    printf("      SET BUZZER ON|OFF，SET BRIGHTNESS 0-100，AUTO\r\n");
}

void App_CommandReceptionStart(void)
{
    command_rx_ready = 0U;
    command_rx_size = 0U;
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, command_rx_buffer,
                                     sizeof(command_rx_buffer)) != HAL_OK)
    {
        Error_Handler();
    }
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
}

void App_CommandRxEvent(uint16_t size)
{
    if (size >= COMMAND_RX_BUFFER_SIZE)
    {
        size = COMMAND_RX_BUFFER_SIZE - 1U;
    }

    command_rx_size = size;
    command_rx_ready = 1U;
}

static void App_ProcessCommand(void)
{
    char *command;
    char *cursor;
    char *end;
    unsigned long value;
    uint16_t size;

    if (command_rx_ready == 0U)
    {
        return;
    }

    size = command_rx_size;
    command_rx_buffer[size] = '\0';
    while ((size > 0U) && ((command_rx_buffer[size - 1U] == '\r') ||
                           (command_rx_buffer[size - 1U] == '\n') ||
                           (command_rx_buffer[size - 1U] == ' ')))
    {
        command_rx_buffer[--size] = '\0';
    }
    command_rx_ready = 0U;
    command = (char *)command_rx_buffer;
    while ((*command == ' ') || (*command == '\r') || (*command == '\n') ||
           (*command == '\t'))
    {
        command++;
    }
    for (cursor = command; *cursor != '\0'; cursor++)
    {
        if ((*cursor >= 'a') && (*cursor <= 'z'))
        {
            *cursor = (char)(*cursor - ('a' - 'A'));
        }
    }

    if (*command == '\0')
    {
        App_CommandReceptionStart();
        return;
    }

    if ((strcmp(command, "GET ALL") == 0) ||
        (strcmp(command, "STATUS") == 0))
    {
        App_PrintAllSensors();
    }
    else if (strcmp(command, "GET TEMP") == 0)
    {
        int32_t temperature_tenths = (int32_t)(latest_temperature * 10.0F);
        printf("温度：%ld.%ld ℃\r\n", (long)(temperature_tenths / 10),
               (long)((temperature_tenths < 0 ? -temperature_tenths : temperature_tenths) % 10));
    }
    else if (strcmp(command, "GET HUM") == 0)
    {
        uint32_t humidity_tenths = (uint32_t)(latest_humidity * 10.0F);
        printf("空气湿度：%lu.%lu %%\r\n", (unsigned long)(humidity_tenths / 10U),
               (unsigned long)(humidity_tenths % 10U));
    }
    else if (strcmp(command, "GET CO2") == 0)
    {
        printf("CO2 浓度：%u ppm\r\n", (unsigned int)latest_co2);
    }
    else if (strcmp(command, "GET LUX") == 0)
    {
        printf("光照强度：%lu lux\r\n", (unsigned long)latest_lux);
    }
    else if (strcmp(command, "GET SOIL") == 0)
    {
        printf("土壤湿度等级：%u\r\n", (unsigned int)latest_soil_level);
    }
    else if (strcmp(command, "SET LAMP ON") == 0)
    {
        automatic_control = 0U;
        App_SetLampPercent(manual_lamp_percent);
        printf("灯已开启，已进入手动模式。\r\n");
    }
    else if (strcmp(command, "SET LAMP OFF") == 0)
    {
        automatic_control = 0U;
        App_SetLampPercent(0U);
        printf("灯已关闭，已进入手动模式。\r\n");
    }
    else if (strcmp(command, "SET LED2 ON") == 0)
    {
        automatic_control = 0U;
        App_SetLed2(1U);
        printf("LED2 已开启，已进入手动模式。\r\n");
    }
    else if (strcmp(command, "SET LED2 OFF") == 0)
    {
        automatic_control = 0U;
        App_SetLed2(0U);
        printf("LED2 已关闭，已进入手动模式。\r\n");
    }
    else if (strcmp(command, "SET RELAY ON") == 0)
    {
        automatic_control = 0U;
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
        printf("继电器已开启，已进入手动模式。\r\n");
    }
    else if (strcmp(command, "SET RELAY OFF") == 0)
    {
        automatic_control = 0U;
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
        printf("继电器已关闭，已进入手动模式。\r\n");
    }
    else if (strcmp(command, "SET BUZZER ON") == 0)
    {
        automatic_control = 0U;
        HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
        printf("蜂鸣器已禁用，保持关闭状态。\r\n");
    }
    else if (strcmp(command, "SET BUZZER OFF") == 0)
    {
        automatic_control = 0U;
        HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
        printf("蜂鸣器已关闭，已进入手动模式。\r\n");
    }
    else if (strncmp(command, "SET BRIGHTNESS ", 15U) == 0)
    {
        value = strtoul(&command[15], &end, 10);
        if ((*end == '\0') && (value <= 100U))
        {
            automatic_control = 0U;
            manual_lamp_percent = (uint8_t)value;
            App_SetLampPercent(manual_lamp_percent);
            printf("灯亮度已设为 %u%%，已进入手动模式。\r\n",
                   (unsigned int)manual_lamp_percent);
        }
        else
        {
            printf("亮度必须是 0 到 100 之间的数字。\r\n");
        }
    }
    else if (strcmp(command, "AUTO") == 0)
    {
        automatic_control = 1U;
        App_ApplyAutomaticControl();
        printf("已恢复自动控制模式。\r\n");
    }
    else if (strcmp(command, "HELP") == 0)
    {
        App_PrintHelp();
    }
    else
    {
        printf("无法识别该命令。\r\n");
        App_PrintHelp();
    }

    App_CommandReceptionStart();
}

void app_main(void)
{
    uint32_t tick_1s = HAL_GetTick();
    uint32_t tick_2s = HAL_GetTick();
    uint32_t tick_adc = HAL_GetTick();
    uint32_t tick_soil = HAL_GetTick();
    uint32_t soil_resistance_ohm = 0U;
    uint8_t ret;
    HAL_StatusTypeDef dma_status;
    HAL_StatusTypeDef adc_status;
    HAL_StatusTypeDef soil_status;

    CO2_UART_Receive_Start();
    DHT22_Init();
    if ((OLED_Init() != HAL_OK) || (OLED_Clear() != HAL_OK) ||
        (OLED_ShowText(0U, 0U, "I2C OK") != HAL_OK) ||
        (OLED_ShowText(0U, 2U, "NUM 123") != HAL_OK))
    {
        Error_Handler();
    }
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    App_SetLampPercent(LIGHT_PWM_MIN_PERCENT);
    App_SetLed2(0U);
    App_CommandReceptionStart();

    dma_status = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_values,
                                   sizeof(adc1_values) / sizeof(adc1_values[0]));
    if (dma_status != HAL_OK)
    {
        Error_Handler();
    }

    while (1)
    {
        App_ProcessCommand();

        if (HAL_GetTick() - tick_adc >= 500U)
        {
            tick_adc = HAL_GetTick();
            adc_status = Light_ReadLux(&latest_lux);
            if (adc_status == HAL_OK)
            {
                lux_valid = 1U;
                App_ApplyAutomaticControl();
            }
        }

        if (HAL_GetTick() - tick_soil >= 1000U)
        {
            tick_soil = HAL_GetTick();
            soil_status = Soil_ReadHumidityLevel(&latest_soil_level,
                                                  &soil_resistance_ohm);
            if (soil_status == HAL_OK)
            {
                soil_valid = 1U;
                App_ApplyAutomaticControl();
            }
        }

        if (HAL_GetTick() - tick_1s >= 1000U)
        {
            tick_1s = HAL_GetTick();
            ret = CO2_get_data(&latest_co2, 1000U);
            if (ret == 0U)
            {
                co2_valid = 1U;
                App_ApplyAutomaticControl();
            }
        }

        if (HAL_GetTick() - tick_2s >= 2000U)
        {
            tick_2s = HAL_GetTick();
            ret = DHT22_ReadData(&latest_temperature, &latest_humidity);
            if (ret == 0U)
            {
                temperature_valid = 1U;
                humidity_valid = 1U;
                if (latest_temperature >= TEMP_ALARM_ON_C)
                {
                    temperature_alarm = 1U;
                }
                else if (latest_temperature <= TEMP_ALARM_OFF_C)
                {
                    temperature_alarm = 0U;
                }
                App_ApplyAutomaticControl();
            }
        }
    }
}
#endif

#include "app_main.h"
#include "main.h"
#include "oled.h"

void app_main(void)
{
    if ((OLED_Init() != HAL_OK) || (OLED_Clear() != HAL_OK) ||
        (OLED_ShowText(0U, 0U, "I2C OK") != HAL_OK) ||
        (OLED_ShowText(0U, 2U, "NUM 123") != HAL_OK) ||
        (OLED_ShowTemperatureIcon(96U, 0U) != HAL_OK))
    {
        Error_Handler();
    }

    while (1)
    {
    }
}
