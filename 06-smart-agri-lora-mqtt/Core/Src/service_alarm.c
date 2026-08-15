#include "service_alarm.h"

#include "service_control.h"
#include "terminal_config.h"

#include <stdio.h>
#include <string.h>

#define SERVICE_ALARM_QUEUE_SIZE 8U

typedef struct
{
    bool pending;
    bool active;
    uint32_t pending_since_ms;
    uint8_t level;
    uint8_t reported_level;
    uint32_t last_report_ms;
} ServiceAlarmState;

static ServiceAlarmState alarm_states[SERVICE_ALARM_COUNT];
static ServiceAlarmEvent alarm_queue[SERVICE_ALARM_QUEUE_SIZE];
static uint8_t alarm_queue_head = 0U;
static uint8_t alarm_queue_count = 0U;
static uint32_t alarm_tick_ms = 0U;

static void AlarmQueuePush(const ServiceAlarmEvent *event)
{
    if (alarm_queue_count >= SERVICE_ALARM_QUEUE_SIZE)
    {
        printf("[ALARM] QUEUE FULL DROP CODE=%u\r\n",
               (unsigned int)event->code);
        return;
    }
    alarm_queue[(alarm_queue_head + alarm_queue_count) %
                SERVICE_ALARM_QUEUE_SIZE] = *event;
    ++alarm_queue_count;
}

static void AlarmUpdate(ServiceAlarmCode code,
                        bool set_condition,
                        bool clear_condition,
                        uint8_t level,
                        uint32_t now)
{
    ServiceAlarmState *state = &alarm_states[code - 1U];
    bool desired = state->active ? !clear_condition : set_condition;
    ServiceAlarmEvent event;

    if (desired != state->pending)
    {
        state->pending = desired;
        state->pending_since_ms = now;
    }

    if ((state->pending != state->active) &&
        ((now - state->pending_since_ms) >= TERMINAL_RULE_CONFIRM_MS))
    {
        state->active = state->pending;
        if (state->active)
        {
            state->level = level;
            state->reported_level = level;
            state->last_report_ms = now;
            event.code = (uint16_t)code;
            event.level = level;
            event.active = 1U;
            AlarmQueuePush(&event);
            printf("[ALARM] RAISE CODE=%u LEVEL=%u\r\n", (unsigned int)code,
                   (unsigned int)level);
        }
        else
        {
            event.code = (uint16_t)code;
            event.level = state->level;
            event.active = 0U;
            AlarmQueuePush(&event);
            printf("[ALARM] CLEAR CODE=%u\r\n", (unsigned int)code);
            state->level = 1U;
            state->reported_level = 1U;
        }
    }
    else if (state->active && (level != state->reported_level))
    {
        /* 等级变化（如 CO2 1500->2000 升级/降级）即时上报 */
        state->level = level;
        state->reported_level = level;
        state->last_report_ms = now;
        event.code = (uint16_t)code;
        event.level = level;
        event.active = 1U;
        AlarmQueuePush(&event);
        printf("[ALARM] LEVEL CODE=%u LEVEL=%u\r\n", (unsigned int)code,
               (unsigned int)level);
    }

    if (state->active && (code == SERVICE_ALARM_SENSOR_FAULT) &&
        ((now - state->last_report_ms) >= TERMINAL_SENSOR_FAULT_REPORT_MS))
    {
        state->last_report_ms = now;
        event.code = (uint16_t)code;
        event.level = state->level;
        event.active = 1U;
        AlarmQueuePush(&event);
        printf("[ALARM] REPEAT CODE=%u\r\n", (unsigned int)code);
    }
}

bool ServiceAlarm_Init(void)
{
    memset(alarm_states, 0, sizeof(alarm_states));
    memset(alarm_queue, 0, sizeof(alarm_queue));
    alarm_queue_head = 0U;
    alarm_queue_count = 0U;
    alarm_tick_ms = ServiceHal_GetTickMs();
    ServiceControl_SetAlarmBuzzerRequest(false);
    printf("[ALARM] INIT OK CONFIRM_MS=%lu BEEP_MS=%lu\r\n",
           (unsigned long)TERMINAL_RULE_CONFIRM_MS,
           (unsigned long)TERMINAL_BEEP_CONFIRM_MS);
    return true;
}

void ServiceAlarm_Process(const TerminalSensorSnapshot *snapshot)
{
    uint32_t now;
    uint16_t status;
    bool temp_cond;
    bool humi_cond;
    bool light_valid;
    bool soil_valid;
    bool co2_valid;
    bool sensor_fault;
    bool beep_request;
    uint8_t co2_level;

    if (snapshot == NULL)
    {
        return;
    }
    now = ServiceHal_GetTickMs();
    if (((int32_t)(now - alarm_tick_ms)) <
        (int32_t)TERMINAL_CONTROL_TICK_MS)
    {
        return;
    }
    alarm_tick_ms = now;

    status = snapshot->device_status;
    light_valid = (status & TERMINAL_SENSOR_STATUS_LIGHT_INVALID) == 0U;
    soil_valid = (status & TERMINAL_SENSOR_STATUS_SOIL_INVALID) == 0U;
    co2_valid = (status & TERMINAL_SENSOR_STATUS_CO2_INVALID) == 0U;
    sensor_fault = (status & TERMINAL_SENSOR_STATUS_ANY_FAULT) != 0U;

    AlarmUpdate(SERVICE_ALARM_LIGHT_LOW,
                light_valid &&
                    (snapshot->lux < TERMINAL_LIGHT_LOW_ON_LUX),
                /* GL5528 查表饱和上限为 350 lux，恢复条件用 >= */
                light_valid &&
                    (snapshot->lux >= TERMINAL_LIGHT_LOW_OFF_LUX),
                1U, now);

    AlarmUpdate(SERVICE_ALARM_SOIL_WET,
                soil_valid &&
                    (snapshot->soil_x10 > TERMINAL_SOIL_WET_ON_X10),
                soil_valid &&
                    (snapshot->soil_x10 < TERMINAL_SOIL_WET_OFF_X10),
                1U, now);

    AlarmUpdate(SERVICE_ALARM_SOIL_DRY,
                soil_valid &&
                    (snapshot->soil_x10 < TERMINAL_SOIL_DRY_ON_X10),
                soil_valid &&
                    (snapshot->soil_x10 > TERMINAL_SOIL_DRY_OFF_X10),
                1U, now);

    co2_level = (co2_valid && (snapshot->co2_ppm > TERMINAL_CO2_DANGER_PPM))
                    ? 2U
                    : 1U;
    AlarmUpdate(SERVICE_ALARM_CO2_HIGH,
                co2_valid &&
                    (snapshot->co2_ppm > TERMINAL_CO2_HIGH_ON_PPM),
                co2_valid &&
                    (snapshot->co2_ppm < TERMINAL_CO2_HIGH_OFF_PPM),
                co2_level, now);

    temp_cond = (status & TERMINAL_SENSOR_STATUS_DHT_INVALID) == 0U &&
                ((snapshot->temperature_x10 < TERMINAL_TEMP_LOW_ON_X10) ||
                 (snapshot->temperature_x10 > TERMINAL_TEMP_HIGH_ON_X10));
    AlarmUpdate(SERVICE_ALARM_TEMP_ALARM, temp_cond,
                (status & TERMINAL_SENSOR_STATUS_DHT_INVALID) == 0U &&
                    (snapshot->temperature_x10 >
                         TERMINAL_TEMP_LOW_OFF_X10) &&
                    (snapshot->temperature_x10 <
                         TERMINAL_TEMP_HIGH_OFF_X10),
                1U, now);

    humi_cond = (status & TERMINAL_SENSOR_STATUS_DHT_INVALID) == 0U &&
                (snapshot->humidity_x10 > TERMINAL_HUMI_HIGH_ON_X10);
    AlarmUpdate(SERVICE_ALARM_HUMI_ALARM, humi_cond,
                (status & TERMINAL_SENSOR_STATUS_DHT_INVALID) == 0U &&
                    (snapshot->humidity_x10 < TERMINAL_HUMI_HIGH_OFF_X10),
                1U, now);

    AlarmUpdate(SERVICE_ALARM_SENSOR_FAULT, sensor_fault, !sensor_fault, 2U,
                now);

    /* 蜂鸣器：温湿度越限持续 6 s 后才响；恢复立即停 */
    beep_request = false;
    if (alarm_states[SERVICE_ALARM_TEMP_ALARM - 1U].pending &&
        ((now -
          alarm_states[SERVICE_ALARM_TEMP_ALARM - 1U].pending_since_ms) >=
         TERMINAL_BEEP_CONFIRM_MS))
    {
        beep_request = true;
    }
    if (alarm_states[SERVICE_ALARM_HUMI_ALARM - 1U].pending &&
        ((now -
          alarm_states[SERVICE_ALARM_HUMI_ALARM - 1U].pending_since_ms) >=
         TERMINAL_BEEP_CONFIRM_MS))
    {
        beep_request = true;
    }
    ServiceControl_SetAlarmBuzzerRequest(beep_request);
}

bool ServiceAlarm_PeekPendingEvent(ServiceAlarmEvent *event)
{
    if ((event == NULL) || (alarm_queue_count == 0U))
    {
        return false;
    }
    *event = alarm_queue[alarm_queue_head];
    return true;
}

void ServiceAlarm_CommitPendingEvent(void)
{
    if (alarm_queue_count == 0U)
    {
        return;
    }
    alarm_queue_head = (alarm_queue_head + 1U) % SERVICE_ALARM_QUEUE_SIZE;
    --alarm_queue_count;
}
