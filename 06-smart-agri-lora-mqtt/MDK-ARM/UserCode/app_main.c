/* 环境监测主流程：CO2、DHT22、光照、土壤采集与 USART1 命令控制。 */
#include "app_main.h"
#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "co2.h"
#include "esp.h"
#include "llcc68_p2p_config.h"
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
#define APP_KEY_LONG_PRESS_MS     1500U
#define APP_SCREEN_TIMEOUT_MS     30000U

/* USART1 printf 重定向：MicroLIB 下 printf 依赖应用提供 fputc。
   main.c 中的原定义位于 #if 0 内，故在此提供，避免重复定义。 */
int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1U, 100U);

    return ch;
}


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

typedef enum
{
    APP_PAGE_HOME = 0,
    APP_PAGE_TEMP_HUMI,
    APP_PAGE_LIGHT_SOIL
} AppPage;

typedef enum
{
    APP_KEY_NONE = 0,
    APP_KEY_CLICK,
    APP_KEY_LONG
} AppKeyEvent;

static AppPage current_page = APP_PAGE_HOME;
static uint8_t page_need_redraw = 1U;   /* 上电首次显示 */
static uint8_t screen_is_on = 1U;
static uint32_t last_operation_tick = 0U;

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

    printf("\r\n--- Sensor Data ---\r\n");
    printf("Temperature: %ld.%ld C\r\n",
           (long)(temperature_tenths / 10),
           (long)((temperature_tenths < 0 ? -temperature_tenths : temperature_tenths) % 10));
    printf("Humidity: %lu.%lu %%\r\n",
           (unsigned long)(humidity_tenths / 10U),
           (unsigned long)(humidity_tenths % 10U));
    printf("CO2: %u ppm\r\n", (unsigned int)latest_co2);
    printf("Light: %lu lux\r\n", (unsigned long)latest_lux);
    printf("Soil moisture level: %u\r\n", (unsigned int)latest_soil_level);
    printf("Control mode: %s\r\n",
           automatic_control ? "AUTO" : "MANUAL");
}

static void App_PrintHelp(void)
{
    printf("Query: GET ALL / GET TEMP / GET HUM / GET CO2 / GET LUX / GET SOIL\r\n");
    printf("Control: SET LAMP ON|OFF, SET LED2 ON|OFF, SET RELAY ON|OFF\r\n");
    printf("         SET BUZZER ON|OFF, SET BRIGHTNESS 0-100, AUTO\r\n");
}

void App_CommandReceptionStart(void)
{
    command_rx_ready = 0U;
    command_rx_size = 0U;
    /* USART1 无 DMA 链路（huart1.hdmarx 未配置），改用空闲中断接收；
       数据经 USART1_IRQHandler -> HAL_UARTEx_RxEventCallback() 上报。 */
    if (HAL_UARTEx_ReceiveToIdle_IT(&huart1, command_rx_buffer,
                                    sizeof(command_rx_buffer)) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USART 空闲/接收完成事件回调（唯一强实现）。
   注意：ISR_callback.c 中存在同名回调，但该文件未加入 Keil 工程；
   若日后将其加入工程，需删除本实现，避免重复定义。 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        App_CommandRxEvent(Size);
    }
    else if (huart->Instance == USART2)
    {
#if ( LLCC68_P2P_ROLE == LLCC68_P2P_ROLE_TX )
        CO2_UART_Callback(Size);
#else
        ESP_UART_Callback(Size);
#endif
    }
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
        printf("Temperature: %ld.%ld C\r\n", (long)(temperature_tenths / 10),
               (long)((temperature_tenths < 0 ? -temperature_tenths : temperature_tenths) % 10));
    }
    else if (strcmp(command, "GET HUM") == 0)
    {
        uint32_t humidity_tenths = (uint32_t)(latest_humidity * 10.0F);
        printf("Humidity: %lu.%lu %%\r\n", (unsigned long)(humidity_tenths / 10U),
               (unsigned long)(humidity_tenths % 10U));
    }
    else if (strcmp(command, "GET CO2") == 0)
    {
        printf("CO2: %u ppm\r\n", (unsigned int)latest_co2);
    }
    else if (strcmp(command, "GET LUX") == 0)
    {
        printf("Light: %lu lux\r\n", (unsigned long)latest_lux);
    }
    else if (strcmp(command, "GET SOIL") == 0)
    {
        printf("Soil moisture level: %u\r\n", (unsigned int)latest_soil_level);
    }
    else if (strcmp(command, "SET LAMP ON") == 0)
    {
        automatic_control = 0U;
        App_SetLampPercent(manual_lamp_percent);
        printf("Lamp ON (manual mode).\r\n");
    }
    else if (strcmp(command, "SET LAMP OFF") == 0)
    {
        automatic_control = 0U;
        App_SetLampPercent(0U);
        printf("Lamp OFF (manual mode).\r\n");
    }
    else if (strcmp(command, "SET LED2 ON") == 0)
    {
        automatic_control = 0U;
        App_SetLed2(1U);
        printf("LED2 ON (manual mode).\r\n");
    }
    else if (strcmp(command, "SET LED2 OFF") == 0)
    {
        automatic_control = 0U;
        App_SetLed2(0U);
        printf("LED2 OFF (manual mode).\r\n");
    }
    else if (strcmp(command, "SET RELAY ON") == 0)
    {
        automatic_control = 0U;
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
        printf("Relay ON (manual mode).\r\n");
    }
    else if (strcmp(command, "SET RELAY OFF") == 0)
    {
        automatic_control = 0U;
        HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
        printf("Relay OFF (manual mode).\r\n");
    }
    else if (strcmp(command, "SET BUZZER ON") == 0)
    {
        automatic_control = 0U;
        HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
        printf("Buzzer disabled, kept OFF.\r\n");
    }
    else if (strcmp(command, "SET BUZZER OFF") == 0)
    {
        automatic_control = 0U;
        HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
        printf("Buzzer OFF (manual mode).\r\n");
    }
    else if (strncmp(command, "SET BRIGHTNESS ", 15U) == 0)
    {
        value = strtoul(&command[15], &end, 10);
        if ((*end == '\0') && (value <= 100U))
        {
            automatic_control = 0U;
            manual_lamp_percent = (uint8_t)value;
            App_SetLampPercent(manual_lamp_percent);
            printf("Lamp brightness set to %u%% (manual mode).\r\n",
                   (unsigned int)manual_lamp_percent);
        }
        else
        {
            printf("Brightness must be 0-100.\r\n");
        }
    }
    else if (strcmp(command, "AUTO") == 0)
    {
        automatic_control = 1U;
        App_ApplyAutomaticControl();
        printf("Auto control restored.\r\n");
    }
    else if (strcmp(command, "HELP") == 0)
    {
        App_PrintHelp();
    }
    else
    {
        printf("Unknown command.\r\n");
        App_PrintHelp();
    }

    App_CommandReceptionStart();
}

/* SW1 按键扫描：非阻塞消抖 20ms + 长按 1500ms。
   状态机：释放 -> 按下(记录 press_tick) -> 保持中(达阈值返回一次 LONG) -> 释放。
   持续按住只产生一次 LONG；长按已触发后释放不补发 CLICK；
   新按下周期开始时重新允许长按。 */
static AppKeyEvent App_KeyScan(void)
{
    static uint8_t key_state = 0U;
    static uint32_t press_tick = 0U;
    static uint8_t long_event_sent = 0U;
    uint8_t key_down;
    AppKeyEvent event = APP_KEY_NONE;

    key_down = (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET) ? 1U : 0U;

    if (key_state == 0U)
    {
        if (key_down != 0U)
        {
            key_state = 1U;
            press_tick = HAL_GetTick();
            long_event_sent = 0U;
        }
    }
    else
    {
        if (key_down != 0U)
        {
            /* 持续按住：首次达到阈值返回 LONG，之后不再重复 */
            if ((long_event_sent == 0U) &&
                ((HAL_GetTick() - press_tick) >= APP_KEY_LONG_PRESS_MS))
            {
                long_event_sent = 1U;
                event = APP_KEY_LONG;
            }
        }
        else
        {
            key_state = 0U;
            if (long_event_sent != 0U)
            {
                /* 长按已触发：释放时不补发短按 */
                return APP_KEY_NONE;
            }
            if ((HAL_GetTick() - press_tick) >= 20U)
            {
                event = APP_KEY_CLICK;
            }
        }
    }
    return event;
}

static void App_HandleKeyEvent(AppKeyEvent event)
{
    if (event == APP_KEY_NONE)
    {
        return;
    }

    /* 有效按键操作：重新开始 30 秒无操作计时 */
    last_operation_tick = HAL_GetTick();

    if (screen_is_on == 0U)
    {
        /* 熄屏状态：首次按键只唤醒，不执行页面切换 */
        screen_is_on = 1U;
        if (event == APP_KEY_LONG)
        {
            current_page = APP_PAGE_HOME;
        }
        page_need_redraw = 1U;
        return;
    }

    if (event == APP_KEY_LONG)
    {
        /* 任意页面长按返回主页；主页长按重绘一次主页 */
        current_page = APP_PAGE_HOME;
        page_need_redraw = 1U;
    }
    else
    {
        if (current_page == APP_PAGE_HOME)
        {
            current_page = APP_PAGE_TEMP_HUMI;
        }
        else if (current_page == APP_PAGE_TEMP_HUMI)
        {
            current_page = APP_PAGE_LIGHT_SOIL;
        }
        else
        {
            current_page = APP_PAGE_TEMP_HUMI;
        }
        page_need_redraw = 1U;
    }
}

/* 30 秒无操作息屏检测：非阻塞，按键保持按下期间暂缓超时。
   熄屏仅清显示，不停止采样、串口与自动控制。 */
static void App_CheckScreenTimeout(void)
{
    if (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
    {
        return;
    }

    if ((screen_is_on != 0U) &&
        ((HAL_GetTick() - last_operation_tick) >= APP_SCREEN_TIMEOUT_MS))
    {
        OLED_Clear();
        screen_is_on = 0U;
        page_need_redraw = 0U;
    }
}

static HAL_StatusTypeDef App_DrawHomePage(void)
{
    OLED_FrameClear();
    OLED_DrawUtf8Text16(32U, 24U, "粤嵌科技");
    return OLED_Refresh();
}

static HAL_StatusTypeDef App_DrawTempHumiPage(void)
{
    char line[32];
    int32_t temp_whole;
    int32_t temp_frac;
    uint32_t humi_tenths;
    HAL_StatusTypeDef status;

    OLED_FrameClear();
    OLED_DrawUtf8Text16(48U, 0U, "温度");
    OLED_DrawUtf8Text16(48U, 24U, "湿度");
    status = OLED_Refresh();
    if (status != HAL_OK)
    {
        return status;
    }

    if ((temperature_valid != 0U) && (humidity_valid != 0U))
    {
        temp_whole = (int32_t)(latest_temperature * 10.0F) / 10;
        temp_frac = (int32_t)(latest_temperature * 10.0F) % 10;
        if (temp_frac < 0)
        {
            temp_frac = -temp_frac;
        }
        humi_tenths = (uint32_t)(latest_humidity * 10.0F);
        sprintf(line, "TEMP:%ld.%ld C", (long)temp_whole, (long)temp_frac);
        status = OLED_ShowText(31U, 2U, line);
        if (status != HAL_OK)
        {
            return status;
        }
        sprintf(line, "HUMI:%lu.%lu %%", (unsigned long)(humi_tenths / 10U),
                (unsigned long)(humi_tenths % 10U));
        return OLED_ShowText(31U, 5U, line);
    }

    status = OLED_ShowText(31U, 2U, "TEMP:--.- C");
    if (status != HAL_OK)
    {
        return status;
    }
    return OLED_ShowText(31U, 5U, "HUMI:--.- %");
}

static HAL_StatusTypeDef App_DrawLightSoilPage(void)
{
    char line[32];
    HAL_StatusTypeDef status;

    OLED_FrameClear();
    OLED_DrawUtf8Text16(48U, 0U, "光照");
    OLED_DrawUtf8Text16(48U, 24U, "土壤");
    status = OLED_Refresh();
    if (status != HAL_OK)
    {
        return status;
    }

    if (lux_valid != 0U)
    {
        sprintf(line, "LIGHT:%lu lux", (unsigned long)latest_lux);
    }
    else
    {
        sprintf(line, "LIGHT:-- lux");
    }
    status = OLED_ShowText(28U, 2U, line);
    if (status != HAL_OK)
    {
        return status;
    }

    if (soil_valid != 0U)
    {
        sprintf(line, "SOIL:LEVEL %u", (unsigned int)latest_soil_level);
    }
    else
    {
        sprintf(line, "SOIL:--");
    }
    return OLED_ShowText(28U, 5U, line);
}

static HAL_StatusTypeDef App_DrawCurrentPage(void)
{
    if (current_page == APP_PAGE_HOME)
    {
        return App_DrawHomePage();
    }
    if (current_page == APP_PAGE_TEMP_HUMI)
    {
        return App_DrawTempHumiPage();
    }
    return App_DrawLightSoilPage();
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
    if (OLED_Init() != HAL_OK)
    {
        Error_Handler();
    }
    /* 启动状态：主页、亮屏；首次显示由 page_need_redraw 在 while(1) 首轮触发。 */
    current_page = APP_PAGE_HOME;
    screen_is_on = 1U;
    page_need_redraw = 1U;
    last_operation_tick = HAL_GetTick();
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
        AppKeyEvent key_event;

        App_ProcessCommand();

        key_event = App_KeyScan();
        if (key_event != APP_KEY_NONE)
        {
            App_HandleKeyEvent(key_event);
        }

        if (HAL_GetTick() - tick_adc >= 500U)
        {
            tick_adc = HAL_GetTick();
            adc_status = Light_ReadLux(&latest_lux);
            if (adc_status == HAL_OK)
            {
                lux_valid = 1U;
                App_ApplyAutomaticControl();
                if (current_page == APP_PAGE_LIGHT_SOIL)
                {
                    page_need_redraw = 1U;
                }
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
                if (current_page == APP_PAGE_LIGHT_SOIL)
                {
                    page_need_redraw = 1U;
                }
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
                App_ApplyAutomaticControl();
                if (current_page == APP_PAGE_TEMP_HUMI)
                {
                    page_need_redraw = 1U;
                }
            }
        }

        App_CheckScreenTimeout();

        if ((screen_is_on != 0U) && (page_need_redraw != 0U))
        {
            page_need_redraw = 0U;
            if (App_DrawCurrentPage() != HAL_OK)
            {
                Error_Handler();
            }
        }
    }
}
