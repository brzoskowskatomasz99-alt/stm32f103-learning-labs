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

#if 0
#include "app_main.h"
#include "main.h"
#include "oled.h"

#define GAME_FRAME_INTERVAL_MS    100U
#define BUTTON_DEBOUNCE_MS        20U
#define DINO_X                    10U
#define DINO_GROUND_Y             36U
#define DINO_WIDTH                16U
#define DINO_HEIGHT               18U
#define OBSTACLE_Y                38U
#define OBSTACLE_WIDTH            15U
#define OBSTACLE_COLLISION_TOP    42U
#define OBSTACLE_START_X          127
#define OBSTACLE_STEP_PIXELS      2
#define LED_FLASH_INTERVAL_MS     80U
#define LED_FLASH_TOGGLE_COUNT    10U
#define SCORE_MAX                 999U

static const uint8_t dino_bitmap[] = {
    0x00U, 0x3CU, 0x00U, 0x7EU, 0x00U, 0x6EU, 0x00U, 0x7EU,
    0x00U, 0x7CU, 0x00U, 0x70U, 0x80U, 0xF0U, 0xC1U, 0xF0U,
    0xE3U, 0xF0U, 0xFFU, 0xF0U, 0x7FU, 0xF0U, 0x3FU, 0xF0U,
    0x1FU, 0xE0U, 0x0FU, 0xC0U, 0x0DU, 0x80U, 0x18U, 0xC0U,
    0x18U, 0xC0U, 0x18U, 0xC0U
};

static const uint8_t jump_offsets[] = {
    0U, 6U, 12U, 18U, 24U,
    30U, 30U, 30U, 30U, 30U, 30U, 30U, 30U, 30U,
    30U, 30U, 30U, 30U, 30U, 30U, 30U, 30U,
    24U, 18U, 12U, 6U, 0U
};

static const uint8_t tiny_digits[10][5] = {
    {7U, 5U, 5U, 5U, 7U},
    {2U, 6U, 2U, 2U, 7U},
    {7U, 1U, 7U, 4U, 7U},
    {7U, 1U, 7U, 1U, 7U},
    {5U, 5U, 7U, 1U, 1U},
    {7U, 4U, 7U, 1U, 7U},
    {7U, 4U, 7U, 5U, 7U},
    {7U, 1U, 2U, 2U, 2U},
    {7U, 5U, 7U, 5U, 7U},
    {7U, 5U, 7U, 1U, 7U}
};

static const uint8_t tiny_s[5] = {7U, 4U, 7U, 1U, 7U};
static const uint8_t tiny_h[5] = {5U, 5U, 7U, 5U, 5U};

static uint8_t App_ButtonPressed(void)
{
    static GPIO_PinState raw_state = GPIO_PIN_SET;
    static GPIO_PinState stable_state = GPIO_PIN_SET;
    static uint32_t changed_at;
    GPIO_PinState sample;
    uint32_t now;

    sample = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);
    now = HAL_GetTick();

    if (sample != raw_state)
    {
        raw_state = sample;
        changed_at = now;
    }

    if ((sample != stable_state) &&
        ((now - changed_at) >= BUTTON_DEBOUNCE_MS))
    {
        stable_state = sample;
        if (stable_state == GPIO_PIN_RESET)
        {
            return 1U;
        }
    }
    return 0U;
}

static void App_DrawClippedRect(int16_t x, uint8_t y, uint8_t width,
                                uint8_t height)
{
    int16_t right;
    int16_t visible_x;
    int16_t visible_right;

    right = x + width;
    if ((right <= 0) || (x >= 128))
    {
        return;
    }

    visible_x = (x < 0) ? 0 : x;
    visible_right = (right > 128) ? 128 : right;
    OLED_FillRect((uint8_t)visible_x, y,
                  (uint8_t)(visible_right - visible_x), height, 1U);
}

static void App_DrawObstacle(int16_t x)
{
    App_DrawClippedRect(x + 5, OBSTACLE_Y, 4U, 18U);
    App_DrawClippedRect(x, 44U, 5U, 4U);
    App_DrawClippedRect(x, 40U, 3U, 8U);
    App_DrawClippedRect(x + 9, 47U, 5U, 4U);
    App_DrawClippedRect(x + 12, 43U, 3U, 8U);
}

static uint8_t App_HasCollision(uint8_t dino_y, int16_t obstacle_x)
{
    int16_t dino_right = (int16_t)DINO_X + (int16_t)DINO_WIDTH - 2;
    int16_t obstacle_right = obstacle_x + (int16_t)OBSTACLE_WIDTH - 2;
    uint8_t dino_bottom = (uint8_t)(dino_y + DINO_HEIGHT - 1U);

    if ((dino_right < (obstacle_x + 1)) ||
        (((int16_t)DINO_X + 2) > obstacle_right))
    {
        return 0U;
    }

    return (dino_bottom >= OBSTACLE_COLLISION_TOP) ? 1U : 0U;
}

static void App_DrawGameOverMark(void)
{
    uint8_t offset;

    for (offset = 0U; offset < 12U; ++offset)
    {
        OLED_DrawPixel((uint8_t)(58U + offset), (uint8_t)(16U + offset), 1U);
        OLED_DrawPixel((uint8_t)(69U - offset), (uint8_t)(16U + offset), 1U);
    }
}

static void App_DrawTinyGlyph(uint8_t x, uint8_t y, const uint8_t glyph[5])
{
    uint8_t row;
    uint8_t column;

    for (row = 0U; row < 5U; ++row)
    {
        for (column = 0U; column < 3U; ++column)
        {
            if ((glyph[row] & (uint8_t)(1U << (2U - column))) != 0U)
            {
                OLED_FillRect((uint8_t)(x + (column * 2U)),
                              (uint8_t)(y + (row * 2U)), 2U, 2U, 1U);
            }
        }
    }
}

static void App_DrawScoreCounter(uint8_t x, const uint8_t label[5],
                                 uint16_t value)
{
    uint8_t hundreds;
    uint8_t tens;
    uint8_t ones;

    value %= 1000U;
    hundreds = (uint8_t)(value / 100U);
    tens = (uint8_t)((value / 10U) % 10U);
    ones = (uint8_t)(value % 10U);

    App_DrawTinyGlyph(x, 2U, label);
    App_DrawTinyGlyph((uint8_t)(x + 8U), 2U, tiny_digits[hundreds]);
    App_DrawTinyGlyph((uint8_t)(x + 16U), 2U, tiny_digits[tens]);
    App_DrawTinyGlyph((uint8_t)(x + 24U), 2U, tiny_digits[ones]);
}

static HAL_StatusTypeDef App_DrawGameScene(uint8_t dino_y,
                                           int16_t obstacle_x,
                                           uint8_t game_over,
                                           uint16_t score,
                                           uint16_t high_score)
{
    OLED_FrameClear();

    App_DrawScoreCounter(2U, tiny_s, score);
    App_DrawScoreCounter(94U, tiny_h, high_score);

    /* 云朵 */
    OLED_FillRect(59U, 9U, 16U, 2U, 1U);
    OLED_FillRect(63U, 6U, 8U, 2U, 1U);

    OLED_DrawBitmap(DINO_X, dino_y, DINO_WIDTH, DINO_HEIGHT, dino_bitmap);

    App_DrawObstacle(obstacle_x);

    /* 地面和碎石 */
    OLED_FillRect(0U, 57U, 128U, 1U, 1U);
    OLED_FillRect(35U, 61U, 5U, 1U, 1U);
    OLED_FillRect(70U, 60U, 3U, 1U, 1U);
    OLED_FillRect(115U, 62U, 4U, 1U, 1U);

    if (game_over != 0U)
    {
        App_DrawGameOverMark();
    }

    return OLED_Refresh();
}

void app_main(void)
{
    uint32_t frame_tick;
    int16_t obstacle_x = OBSTACLE_START_X;
    uint8_t dino_y = DINO_GROUND_Y;
    uint8_t jump_step = 0U;
    uint8_t jump_active = 0U;
    uint8_t game_over = 0U;
    uint8_t button_pressed;
    uint8_t obstacle_scored = 0U;
    uint8_t led_flash_toggles = 0U;
    uint16_t score = 0U;
    uint16_t high_score = 0U;
    uint32_t led_flash_tick = 0U;

    if (OLED_Init() != HAL_OK)
    {
        Error_Handler();
    }
    if (App_DrawGameScene(dino_y, obstacle_x, game_over,
                          score, high_score) != HAL_OK)
    {
        Error_Handler();
    }
    frame_tick = HAL_GetTick();

    while (1)
    {
        if ((led_flash_toggles != 0U) &&
            ((HAL_GetTick() - led_flash_tick) >= LED_FLASH_INTERVAL_MS))
        {
            led_flash_tick = HAL_GetTick();
            HAL_GPIO_TogglePin(LED3_GPIO_Port, LED3_Pin);
            --led_flash_toggles;
            if (led_flash_toggles == 0U)
            {
                HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);
            }
        }

        button_pressed = App_ButtonPressed();
        if (button_pressed != 0U)
        {
            if (game_over != 0U)
            {
                obstacle_x = OBSTACLE_START_X;
                dino_y = DINO_GROUND_Y;
                jump_step = 0U;
                jump_active = 0U;
                game_over = 0U;
                obstacle_scored = 0U;
                score = 0U;
                led_flash_toggles = 0U;
                HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET);

                if (App_DrawGameScene(dino_y, obstacle_x, game_over,
                                      score, high_score) != HAL_OK)
                {
                    Error_Handler();
                }
                frame_tick = HAL_GetTick();
            }
            else if (jump_active == 0U)
            {
                jump_active = 1U;
                jump_step = 0U;
            }
        }

        if ((game_over == 0U) &&
            ((HAL_GetTick() - frame_tick) >= GAME_FRAME_INTERVAL_MS))
        {
            frame_tick = HAL_GetTick();

            obstacle_x -= OBSTACLE_STEP_PIXELS;
            if ((obstacle_scored == 0U) &&
                ((obstacle_x + (int16_t)OBSTACLE_WIDTH) < (int16_t)DINO_X))
            {
                if (score < SCORE_MAX)
                {
                    ++score;
                }
                if (score > high_score)
                {
                    high_score = score;
                }
                obstacle_scored = 1U;
            }
            if (obstacle_x <= -(int16_t)OBSTACLE_WIDTH)
            {
                obstacle_x = OBSTACLE_START_X;
                obstacle_scored = 0U;
            }

            if (jump_active != 0U)
            {
                if (jump_step <
                    ((sizeof(jump_offsets) / sizeof(jump_offsets[0])) - 1U))
                {
                    ++jump_step;
                }
                else
                {
                    jump_step = 0U;
                    jump_active = 0U;
                }
                dino_y = (uint8_t)(DINO_GROUND_Y - jump_offsets[jump_step]);
            }

            if (App_HasCollision(dino_y, obstacle_x) != 0U)
            {
                game_over = 1U;
                led_flash_toggles = LED_FLASH_TOGGLE_COUNT;
                led_flash_tick = HAL_GetTick();
                HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, GPIO_PIN_SET);
            }

            if (App_DrawGameScene(dino_y, obstacle_x, game_over,
                                  score, high_score) != HAL_OK)
            {
                Error_Handler();
            }
        }
    }
}
#endif

#include "app_main.h"
#include "bmp.h"
#include "oled.h"

#define GIF_FRAME_INTERVAL_MS  150U

static HAL_StatusTypeDef App_DrawMouldingDemo(uint8_t gif_frame)
{
    OLED_FrameClear();
    OLED_DrawUtf8Text16(0U, 0U, "您好陈工");
    OLED_DrawBitmap(8U, 24U, 32U, 32U, g_image_dot_1_24bit_32x32);
    OLED_DrawBitmap(80U, 24U, 32U, 32U,
                    g_gif_astronaut_frames[gif_frame]);
    return OLED_Refresh();
}

void app_main(void)
{
    uint8_t gif_frame = 0U;
    uint32_t frame_tick;

    if (OLED_Init() != HAL_OK)
    {
        Error_Handler();
    }
    if (App_DrawMouldingDemo(gif_frame) != HAL_OK)
    {
        Error_Handler();
    }
    frame_tick = HAL_GetTick();

    while (1)
    {
        if ((HAL_GetTick() - frame_tick) >= GIF_FRAME_INTERVAL_MS)
        {
            frame_tick = HAL_GetTick();
            gif_frame = (uint8_t)((gif_frame + 1U) % GIF_ASTRONAUT_FRAME_COUNT);
            if (App_DrawMouldingDemo(gif_frame) != HAL_OK)
            {
                Error_Handler();
            }
        }
    }
}
