#include "terminal_sensors.h"

#include "adc.h"
#include "co2.h"
#include "dht22.h"
#include "light.h"
#include "soil.h"
#include "terminal_config.h"

#include <string.h>

#define TERMINAL_SENSOR_DHT_INTERVAL_MS  2000U
#define TERMINAL_SENSOR_ADC_INTERVAL_MS   500U
#define TERMINAL_SENSOR_CO2_STALE_MS      5000U

typedef struct
{
    int16_t samples[TERMINAL_FILTER_WINDOW];
    uint8_t count;
    uint8_t index;
} TerminalFilterInt16;

typedef struct
{
    uint16_t samples[TERMINAL_FILTER_WINDOW];
    uint8_t count;
    uint8_t index;
} TerminalFilterUint16;

static TerminalSensorSnapshot terminal_snapshot;
static bool terminal_sensors_initialized = false;
static bool terminal_adc_running = false;
static uint32_t terminal_dht_tick = 0U;
static uint32_t terminal_adc_tick = 0U;
static uint32_t terminal_co2_valid_tick = 0U;

static TerminalFilterUint16 filter_lux;
static TerminalFilterUint16 filter_soil;
static TerminalFilterUint16 filter_humi;
static TerminalFilterInt16 filter_temp;
static uint8_t fail_dht = 0U;
static uint8_t fail_co2 = 0U;
static uint8_t fail_light = 0U;
static uint8_t fail_soil = 0U;

static void TerminalSensors_SetStatus(uint16_t mask, bool active)
{
    if (active)
    {
        terminal_snapshot.device_status |= mask;
    }
    else
    {
        terminal_snapshot.device_status &= (uint16_t)(~mask);
    }
}

static void FilterUint16Push(TerminalFilterUint16 *filter, uint16_t value)
{
    filter->samples[filter->index] = value;
    filter->index = (uint8_t)((filter->index + 1U) % TERMINAL_FILTER_WINDOW);
    if (filter->count < TERMINAL_FILTER_WINDOW)
    {
        ++filter->count;
    }
}

static uint16_t FilterUint16Average(const TerminalFilterUint16 *filter)
{
    uint32_t sum = 0U;
    uint8_t index;

    if (filter->count == 0U)
    {
        return 0U;
    }
    for (index = 0U; index < filter->count; ++index)
    {
        sum += filter->samples[index];
    }
    return (uint16_t)(sum / filter->count);
}

static void FilterInt16Push(TerminalFilterInt16 *filter, int16_t value)
{
    filter->samples[filter->index] = value;
    filter->index = (uint8_t)((filter->index + 1U) % TERMINAL_FILTER_WINDOW);
    if (filter->count < TERMINAL_FILTER_WINDOW)
    {
        ++filter->count;
    }
}

static int16_t FilterInt16Average(const TerminalFilterInt16 *filter)
{
    int32_t sum = 0;
    uint8_t index;

    if (filter->count == 0U)
    {
        return 0;
    }
    for (index = 0U; index < filter->count; ++index)
    {
        sum += filter->samples[index];
    }
    return (int16_t)(sum / (int32_t)filter->count);
}

/* 连续失败计数：达到 TERMINAL_SENSOR_FAIL_COUNT 置故障位，成功即清除 */
static void TerminalSensors_RecordResult(uint16_t fault_mask,
                                         uint8_t *counter,
                                         bool ok)
{
    if (ok)
    {
        *counter = 0U;
        TerminalSensors_SetStatus(fault_mask, false);
    }
    else if (*counter < 0xFFU)
    {
        ++(*counter);
        if (*counter >= TERMINAL_SENSOR_FAIL_COUNT)
        {
            TerminalSensors_SetStatus(fault_mask, true);
        }
    }
}

bool TerminalSensors_Init(void)
{
    bool init_ok = true;

    memset(&terminal_snapshot, 0, sizeof(terminal_snapshot));
    terminal_snapshot.device_status = TERMINAL_SENSOR_STATUS_DHT_INVALID |
                                      TERMINAL_SENSOR_STATUS_CO2_INVALID |
                                      TERMINAL_SENSOR_STATUS_LIGHT_INVALID |
                                      TERMINAL_SENSOR_STATUS_SOIL_INVALID;
    memset(&filter_lux, 0, sizeof(filter_lux));
    memset(&filter_soil, 0, sizeof(filter_soil));
    memset(&filter_humi, 0, sizeof(filter_humi));
    memset(&filter_temp, 0, sizeof(filter_temp));
    fail_dht = 0U;
    fail_co2 = 0U;
    fail_light = 0U;
    fail_soil = 0U;

    DHT22_Init();
    CO2_UART_Receive_Start();

    if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
    {
        TerminalSensors_SetStatus(TERMINAL_SENSOR_STATUS_ADC_FAULT, true);
        init_ok = false;
    }

    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_values, 2U) == HAL_OK)
    {
        terminal_adc_running = true;
    }
    else
    {
        terminal_adc_running = false;
        TerminalSensors_SetStatus(TERMINAL_SENSOR_STATUS_ADC_FAULT, true);
        init_ok = false;
    }

    terminal_dht_tick = HAL_GetTick();
    terminal_adc_tick = HAL_GetTick();
    terminal_co2_valid_tick = HAL_GetTick();
    terminal_sensors_initialized = true;
    return init_ok;
}

void TerminalSensors_Process(void)
{
    uint32_t now;
    uint8_t co2_status;
    uint16_t co2_ppm;

    if (!terminal_sensors_initialized)
    {
        return;
    }

    now = HAL_GetTick();

    co2_status = CO2_TakeLatest(&co2_ppm);
    if (co2_status == CO2_STATUS_OK)
    {
        if ((co2_ppm > 0U) && (co2_ppm <= TERMINAL_CO2_MAX_PPM))
        {
            terminal_snapshot.co2_ppm = co2_ppm;
            terminal_co2_valid_tick = now;
            TerminalSensors_SetStatus(TERMINAL_SENSOR_STATUS_CO2_INVALID, false);
            TerminalSensors_RecordResult(TERMINAL_SENSOR_STATUS_CO2_FAULT,
                                         &fail_co2, true);
        }
        else
        {
            TerminalSensors_SetStatus(TERMINAL_SENSOR_STATUS_CO2_INVALID, true);
            TerminalSensors_RecordResult(TERMINAL_SENSOR_STATUS_CO2_FAULT,
                                         &fail_co2, false);
        }
    }
    else if (co2_status != CO2_STATUS_NO_DATA)
    {
        TerminalSensors_SetStatus(TERMINAL_SENSOR_STATUS_CO2_INVALID, true);
        TerminalSensors_RecordResult(TERMINAL_SENSOR_STATUS_CO2_FAULT,
                                     &fail_co2, false);
    }
    else if ((now - terminal_co2_valid_tick) >= TERMINAL_SENSOR_CO2_STALE_MS)
    {
        TerminalSensors_SetStatus(TERMINAL_SENSOR_STATUS_CO2_INVALID, true);
        TerminalSensors_RecordResult(TERMINAL_SENSOR_STATUS_CO2_FAULT,
                                     &fail_co2, false);
    }

    if (terminal_adc_running &&
        ((now - terminal_adc_tick) >= TERMINAL_SENSOR_ADC_INTERVAL_MS))
    {
        uint16_t lux;
        uint16_t soil_x10;

        terminal_adc_tick = now;

        if ((adc1_values[0] < 4095U) && (Light_ReadLux(&lux) == HAL_OK))
        {
            FilterUint16Push(&filter_lux, lux);
            terminal_snapshot.lux = FilterUint16Average(&filter_lux);
            TerminalSensors_SetStatus(TERMINAL_SENSOR_STATUS_LIGHT_INVALID, false);
            TerminalSensors_RecordResult(TERMINAL_SENSOR_STATUS_LIGHT_FAULT,
                                         &fail_light, true);
        }
        else
        {
            TerminalSensors_SetStatus(TERMINAL_SENSOR_STATUS_LIGHT_INVALID, true);
            TerminalSensors_RecordResult(TERMINAL_SENSOR_STATUS_LIGHT_FAULT,
                                         &fail_light, false);
        }

        if (Soil_ReadHumidityX10(&soil_x10) == HAL_OK)
        {
            FilterUint16Push(&filter_soil, soil_x10);
            terminal_snapshot.soil_x10 = FilterUint16Average(&filter_soil);
            TerminalSensors_SetStatus(TERMINAL_SENSOR_STATUS_SOIL_INVALID, false);
            TerminalSensors_RecordResult(TERMINAL_SENSOR_STATUS_SOIL_FAULT,
                                         &fail_soil, true);
        }
        else
        {
            TerminalSensors_SetStatus(TERMINAL_SENSOR_STATUS_SOIL_INVALID, true);
            TerminalSensors_RecordResult(TERMINAL_SENSOR_STATUS_SOIL_FAULT,
                                         &fail_soil, false);
        }
    }

    if ((now - terminal_dht_tick) >= TERMINAL_SENSOR_DHT_INTERVAL_MS)
    {
        float temperature;
        float humidity;

        terminal_dht_tick = now;
        if ((DHT22_ReadData(&temperature, &humidity) == 0U) &&
            (temperature >= -40.0F) && (temperature <= 80.0F) &&
            (humidity >= 0.0F) && (humidity <= 100.0F))
        {
            FilterInt16Push(&filter_temp, (int16_t)(temperature * 10.0F));
            FilterUint16Push(&filter_humi, (uint16_t)(humidity * 10.0F));
            terminal_snapshot.temperature_x10 = FilterInt16Average(&filter_temp);
            terminal_snapshot.humidity_x10 = FilterUint16Average(&filter_humi);
            TerminalSensors_SetStatus(TERMINAL_SENSOR_STATUS_DHT_INVALID, false);
            TerminalSensors_RecordResult(TERMINAL_SENSOR_STATUS_DHT_FAULT,
                                         &fail_dht, true);
        }
        else
        {
            TerminalSensors_SetStatus(TERMINAL_SENSOR_STATUS_DHT_INVALID, true);
            TerminalSensors_RecordResult(TERMINAL_SENSOR_STATUS_DHT_FAULT,
                                         &fail_dht, false);
        }
    }
}

bool TerminalSensors_GetSnapshot(TerminalSensorSnapshot *snapshot)
{
    if ((snapshot == NULL) || !terminal_sensors_initialized)
    {
        return false;
    }

    *snapshot = terminal_snapshot;
    return true;
}
