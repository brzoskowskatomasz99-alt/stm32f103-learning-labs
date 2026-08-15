#ifndef SERVICE_ALARM_H
#define SERVICE_ALARM_H

#include <stdbool.h>
#include <stdint.h>

#include "terminal_sensors.h"

/*
 * service_alarm：告警状态机（任务书第 6/8 章）。
 * 去重（仅状态变化时上报）、恢复事件、SENSOR_FAULT 60 s 限频、
 * 温湿度蜂鸣器 6 s 确认延时。事件由 terminal_autonomy 消费并组 LoRa 告警帧。
 */

typedef enum
{
    SERVICE_ALARM_LIGHT_LOW = 1U,
    SERVICE_ALARM_SOIL_WET = 2U,
    SERVICE_ALARM_SOIL_DRY = 3U,
    SERVICE_ALARM_CO2_HIGH = 4U,
    SERVICE_ALARM_TEMP_ALARM = 5U,
    SERVICE_ALARM_HUMI_ALARM = 6U,
    SERVICE_ALARM_SENSOR_FAULT = 7U,
    SERVICE_ALARM_COUNT = 7U
} ServiceAlarmCode;

typedef struct
{
    uint16_t code;
    uint8_t level;
    uint8_t active; /* 1=告警发生，0=恢复 */
} ServiceAlarmEvent;

bool ServiceAlarm_Init(void);

/* 每 TERMINAL_CONTROL_TICK_MS 调用一次 */
void ServiceAlarm_Process(const TerminalSensorSnapshot *snapshot);

/* 窥视队头事件（不消费）；发送成功后再 Commit */
bool ServiceAlarm_PeekPendingEvent(ServiceAlarmEvent *event);
void ServiceAlarm_CommitPendingEvent(void);

#endif /* SERVICE_ALARM_H */
