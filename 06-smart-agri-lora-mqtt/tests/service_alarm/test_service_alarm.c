/* service_alarm 主机单元测试：桩实现 service_hal 与蜂鸣器请求接口。 */
#include "service_alarm.h"
#include "service_hal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
    do                                                                          \
    {                                                                           \
        if (!(condition))                                                       \
        {                                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);        \
            return 1;                                                           \
        }                                                                       \
    } while (0)

/* ---- 桩 ---- */
static uint32_t g_now_ms = 0U;
static int g_buzzer_request = -1;

void ServiceHal_Init(void)
{
}

uint32_t ServiceHal_GetTickMs(void)
{
    return g_now_ms;
}

void ServiceHal_ActuatorWrite(ServiceActuator actuator, uint8_t value)
{
    (void)actuator;
    (void)value;
}

uint8_t ServiceHal_ActuatorRead(ServiceActuator actuator)
{
    (void)actuator;
    return 0U;
}

void ServiceControl_SetAlarmBuzzerRequest(bool on)
{
    g_buzzer_request = on ? 1 : 0;
}

/* ---- 工具 ---- */
static void AdvanceMs(uint32_t ms)
{
    g_now_ms += ms;
}

static void Tick(const TerminalSensorSnapshot *snapshot)
{
    AdvanceMs(500U);
    ServiceAlarm_Process(snapshot);
}

static TerminalSensorSnapshot ValidSnapshot(void)
{
    TerminalSensorSnapshot snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.temperature_x10 = 250;
    snapshot.humidity_x10 = 500;
    snapshot.co2_ppm = 600;
    snapshot.lux = 500;
    snapshot.soil_x10 = 500;
    snapshot.device_status = 0U;
    return snapshot;
}

/* 收集事件（peek/commit） */
static unsigned int Drain(ServiceAlarmEvent *events, unsigned int capacity)
{
    unsigned int count = 0U;

    while (ServiceAlarm_PeekPendingEvent(&events[count]))
    {
        ServiceAlarm_CommitPendingEvent();
        ++count;
        if (count >= capacity)
        {
            break;
        }
    }
    return count;
}

static int test_temp_raise_dedup_and_clear(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();
    ServiceAlarmEvent events[8];
    unsigned int count;

    CHECK(ServiceAlarm_Init() == true);
    snapshot.temperature_x10 = 360; /* 36.0 C > 35 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 0U); /* 1.5 s 内未确认 */
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 1U);
    CHECK(events[0].code == SERVICE_ALARM_TEMP_ALARM);
    CHECK(events[0].level == 1U);
    CHECK(events[0].active == 1U);

    /* 持续超限 10 s：去重，无新事件 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 0U);

    /* 恢复：回到 25 C 持续 1.5 s 后出恢复事件 */
    snapshot.temperature_x10 = 250;
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 0U);
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 1U);
    CHECK(events[0].code == SERVICE_ALARM_TEMP_ALARM);
    CHECK(events[0].active == 0U);
    return 0;
}

static int test_buzzer_6s_confirm_delay(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();
    ServiceAlarmEvent events[8];

    CHECK(ServiceAlarm_Init() == true);
    snapshot.temperature_x10 = 360;
    Tick(&snapshot); /* 500 ms */
    (void)Drain(events, 8U);
    CHECK(g_buzzer_request == 0);
    Tick(&snapshot); /* 1000 */
    Tick(&snapshot); /* 1500 */
    Tick(&snapshot); /* 2000 */
    Tick(&snapshot); /* 2500 */
    Tick(&snapshot); /* 3000 */
    Tick(&snapshot); /* 3500 */
    Tick(&snapshot); /* 4000 */
    Tick(&snapshot); /* 4500 */
    Tick(&snapshot); /* 5000 */
    Tick(&snapshot); /* 5500 */
    CHECK(g_buzzer_request == 0); /* < 6 s 不响 */
    Tick(&snapshot); /* 6000 */
    Tick(&snapshot); /* 6500：pending 起 6000 ms */
    CHECK(g_buzzer_request == 1);

    snapshot.temperature_x10 = 250;
    Tick(&snapshot);
    CHECK(g_buzzer_request == 0); /* 恢复立即停 */
    return 0;
}

static int test_humi_alarm(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();
    ServiceAlarmEvent events[8];
    unsigned int count;

    CHECK(ServiceAlarm_Init() == true);
    snapshot.humidity_x10 = 950; /* > 90% */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 1U);
    CHECK(events[0].code == SERVICE_ALARM_HUMI_ALARM);
    CHECK(events[0].active == 1U);
    return 0;
}

static int test_co2_level_upgrade_and_downgrade(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();
    ServiceAlarmEvent events[8];
    unsigned int count;

    CHECK(ServiceAlarm_Init() == true);
    snapshot.co2_ppm = 1600U; /* > 1500 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 1U);
    CHECK(events[0].code == SERVICE_ALARM_CO2_HIGH);
    CHECK(events[0].level == 1U);
    CHECK(events[0].active == 1U);

    snapshot.co2_ppm = 2100U; /* > 2000：升级 */
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 1U);
    CHECK(events[0].code == SERVICE_ALARM_CO2_HIGH);
    CHECK(events[0].level == 2U);
    CHECK(events[0].active == 1U);

    snapshot.co2_ppm = 1600U; /* 降级 */
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 1U);
    CHECK(events[0].level == 1U);
    CHECK(events[0].active == 1U);

    snapshot.co2_ppm = 1200U; /* 恢复 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 1U);
    CHECK(events[0].active == 0U);
    return 0;
}

static int test_sensor_fault_rate_limited_60s(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();
    ServiceAlarmEvent events[8];
    unsigned int count;
    unsigned int i;

    CHECK(ServiceAlarm_Init() == true);
    snapshot.device_status |= TERMINAL_SENSOR_STATUS_ANY_FAULT;
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 1U);
    CHECK(events[0].code == SERVICE_ALARM_SENSOR_FAULT);
    CHECK(events[0].level == 2U);
    CHECK(events[0].active == 1U);

    /* 持续 70 s：60 s 处仅再上报 1 次（限频），共 2 次 */
    for (i = 0U; i < 140U; ++i) /* 70 s */
    {
        Tick(&snapshot);
    }
    count = Drain(events, 8U);
    CHECK(count == 1U);
    CHECK(events[0].code == SERVICE_ALARM_SENSOR_FAULT);
    CHECK(events[0].active == 1U);

    /* 故障消除：恢复事件 */
    snapshot.device_status &= (uint16_t)(~TERMINAL_SENSOR_STATUS_ANY_FAULT);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 0U);
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 1U);
    CHECK(events[0].code == SERVICE_ALARM_SENSOR_FAULT);
    CHECK(events[0].active == 0U);
    return 0;
}

static int test_peek_does_not_consume(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();
    ServiceAlarmEvent event;

    CHECK(ServiceAlarm_Init() == true);
    snapshot.humidity_x10 = 950;
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceAlarm_PeekPendingEvent(&event) == true);
    CHECK(ServiceAlarm_PeekPendingEvent(&event) == true); /* 未消费 */
    ServiceAlarm_CommitPendingEvent();
    CHECK(ServiceAlarm_PeekPendingEvent(&event) == false);
    ServiceAlarm_Process(NULL); /* 空指针安全返回 */
    return 0;
}

static int test_light_low_raise_and_saturation_clear(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();
    ServiceAlarmEvent events[8];
    unsigned int count;

    CHECK(ServiceAlarm_Init() == true);
    snapshot.lux = 250U;
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 1U);
    CHECK(events[0].code == SERVICE_ALARM_LIGHT_LOW);
    CHECK(events[0].active == 1U);

    /* GL5528 亮光饱和 350 lux 必须能恢复 */
    snapshot.lux = 350U;
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    count = Drain(events, 8U);
    CHECK(count == 1U);
    CHECK(events[0].code == SERVICE_ALARM_LIGHT_LOW);
    CHECK(events[0].active == 0U);
    return 0;
}

int main(void)
{
    CHECK(test_temp_raise_dedup_and_clear() == 0);
    CHECK(test_buzzer_6s_confirm_delay() == 0);
    CHECK(test_humi_alarm() == 0);
    CHECK(test_co2_level_upgrade_and_downgrade() == 0);
    CHECK(test_sensor_fault_rate_limited_60s() == 0);
    CHECK(test_peek_does_not_consume() == 0);
    CHECK(test_light_low_raise_and_saturation_clear() == 0);
    puts("PASS service_alarm");
    return 0;
}
