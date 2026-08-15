#ifndef TERMINAL_SENSORS_H
#define TERMINAL_SENSORS_H

#include <stdbool.h>
#include <stdint.h>

#define TERMINAL_SENSOR_STATUS_DHT_INVALID    0x0001U
#define TERMINAL_SENSOR_STATUS_CO2_INVALID    0x0002U
#define TERMINAL_SENSOR_STATUS_LIGHT_INVALID  0x0004U
#define TERMINAL_SENSOR_STATUS_SOIL_INVALID   0x0008U
#define TERMINAL_SENSOR_STATUS_ADC_FAULT      0x0010U
/* 故障位：连续 3 个采集周期失败后置位，恢复有效读数后清除 */
#define TERMINAL_SENSOR_STATUS_DHT_FAULT      0x0020U
#define TERMINAL_SENSOR_STATUS_CO2_FAULT      0x0040U
#define TERMINAL_SENSOR_STATUS_LIGHT_FAULT    0x0080U
#define TERMINAL_SENSOR_STATUS_SOIL_FAULT     0x0100U
#define TERMINAL_SENSOR_STATUS_ANY_FAULT \
    (TERMINAL_SENSOR_STATUS_DHT_FAULT | TERMINAL_SENSOR_STATUS_CO2_FAULT | \
     TERMINAL_SENSOR_STATUS_LIGHT_FAULT | TERMINAL_SENSOR_STATUS_SOIL_FAULT)

typedef struct
{
    int16_t temperature_x10;
    uint16_t humidity_x10;
    uint16_t co2_ppm;
    uint16_t lux;
    uint16_t soil_x10;
    uint16_t device_status;
} TerminalSensorSnapshot;

bool TerminalSensors_Init(void);
void TerminalSensors_Process(void);
bool TerminalSensors_GetSnapshot(TerminalSensorSnapshot *snapshot);

#endif
