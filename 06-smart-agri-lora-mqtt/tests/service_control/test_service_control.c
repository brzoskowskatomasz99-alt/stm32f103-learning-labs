/* service_control 主机单元测试：桩实现 service_hal，驱动虚拟时钟。 */
#include "service_control.h"

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

/* ---- service_hal 桩 ---- */
static uint32_t g_now_ms = 0U;
static uint8_t g_actuator[SERVICE_ACT_COUNT];
static uint8_t g_initialized = 0U;

void ServiceHal_Init(void)
{
    uint8_t i;
    for (i = 0U; i < SERVICE_ACT_COUNT; ++i)
    {
        g_actuator[i] = 0U;
    }
    g_initialized = 1U;
}

uint32_t ServiceHal_GetTickMs(void)
{
    return g_now_ms;
}

void ServiceHal_ActuatorWrite(ServiceActuator actuator, uint8_t value)
{
    if (actuator < SERVICE_ACT_COUNT)
    {
        g_actuator[actuator] = value;
    }
}

uint8_t ServiceHal_ActuatorRead(ServiceActuator actuator)
{
    if (actuator >= SERVICE_ACT_COUNT)
    {
        return 0U;
    }
    return g_actuator[actuator];
}

/* ---- 工具 ---- */
static void AdvanceMs(uint32_t ms)
{
    g_now_ms += ms;
}

static void Tick(const TerminalSensorSnapshot *snapshot)
{
    AdvanceMs(500U);
    ServiceControl_Process(snapshot);
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

static int test_init_defaults_off(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();

    ServiceHal_Init(); /* 固件侧由 terminal_autonomy 先调 */
    CHECK(g_initialized == 1U);
    CHECK(ServiceControl_Init() == true);
    Tick(&snapshot);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_LIGHT_PWM) ==
          SERVICE_CONTROL_SOURCE_OFF);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_RELAY) ==
          SERVICE_CONTROL_SOURCE_OFF);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_FAN_PWM) ==
          SERVICE_CONTROL_SOURCE_OFF);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_LED_STATUS) ==
          SERVICE_CONTROL_SOURCE_OFF);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_BUZZER) ==
          SERVICE_CONTROL_SOURCE_OFF);
    CHECK(g_actuator[SERVICE_ACT_LIGHT_PWM] == 0U);
    CHECK(g_actuator[SERVICE_ACT_RELAY] == 0U);
    CHECK(g_actuator[SERVICE_ACT_FAN_PWM] == 0U);
    return 0;
}

static int test_light_low_auto_with_confirm(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();
    uint16_t actual = 0U;

    CHECK(ServiceControl_Init() == true);
    snapshot.lux = 250U; /* < 300 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LIGHT_PWM) == 0U);
    Tick(&snapshot); /* 第 4 拍：1.5 s 确认后动作 */
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LIGHT_PWM) == 100U);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_LIGHT_PWM) ==
          SERVICE_CONTROL_SOURCE_AUTO);
    (void)actual;
    return 0;
}

static int test_light_hysteresis_holds_between_thresholds(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();

    CHECK(ServiceControl_Init() == true);
    snapshot.lux = 250U;
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LIGHT_PWM) == 100U);

    snapshot.lux = 340U; /* 300..350 之间：保持 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LIGHT_PWM) == 100U);

    snapshot.lux = 400U; /* > 350：恢复 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LIGHT_PWM) == 0U);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_LIGHT_PWM) ==
          SERVICE_CONTROL_SOURCE_OFF);

    /* 边界：传感器饱和上限 350 必须能恢复（GL5528 真机回归） */
    snapshot.lux = 250U;
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LIGHT_PWM) == 100U);
    snapshot.lux = 350U; /* 恰为恢复阈值 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LIGHT_PWM) == 0U);
    return 0;
}

static int test_manual_override_and_expiry(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();
    uint16_t actual = 0U;

    CHECK(ServiceControl_Init() == true);
    CHECK(ServiceControl_ApplyManualCommand(SERVICE_ACT_LIGHT_PWM, 40U,
                                            &actual) == true);
    CHECK(actual == 40U);
    Tick(&snapshot);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_LIGHT_PWM) ==
          SERVICE_CONTROL_SOURCE_MANUAL);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LIGHT_PWM) == 40U);

    /* 30 min 过期后恢复自动/关 */
    AdvanceMs(1800000U);
    Tick(&snapshot);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_LIGHT_PWM) !=
          SERVICE_CONTROL_SOURCE_MANUAL);
    return 0;
}

static int test_safety_overrides_manual(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();
    uint16_t actual = 0U;

    CHECK(ServiceControl_Init() == true);
    CHECK(ServiceControl_ApplyManualCommand(SERVICE_ACT_LIGHT_PWM, 90U,
                                            &actual) == true);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LIGHT_PWM) == 90U);

    snapshot.device_status |= TERMINAL_SENSOR_STATUS_LIGHT_FAULT;
    Tick(&snapshot);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_LIGHT_PWM) ==
          SERVICE_CONTROL_SOURCE_SAFE);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LIGHT_PWM) == 0U);

    snapshot.device_status &= (uint16_t)~TERMINAL_SENSOR_STATUS_LIGHT_FAULT;
    Tick(&snapshot);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_LIGHT_PWM) ==
          SERVICE_CONTROL_SOURCE_MANUAL);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LIGHT_PWM) == 90U);
    return 0;
}

static int test_soil_dry_turns_relay_on(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();

    CHECK(ServiceControl_Init() == true);
    snapshot.soil_x10 = 350U; /* < 40% */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_RELAY) ==
          SERVICE_CONTROL_SOURCE_AUTO);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_RELAY) == 100U);

    snapshot.soil_x10 = 420U; /* 40..45 之间：保持 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_RELAY) == 100U);

    snapshot.soil_x10 = 500U; /* > 45%：恢复关闭 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_RELAY) == 0U);
    return 0;
}

static int test_soil_wet_keeps_relay_off(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();
    uint16_t actual = 0U;

    CHECK(ServiceControl_Init() == true);
    CHECK(ServiceControl_ApplyManualCommand(SERVICE_ACT_RELAY, 100U,
                                            &actual) == true);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_RELAY) == 100U);

    /* 手动未过期：土壤过湿的自动规则不能改手动（P3 > P2） */
    snapshot.soil_x10 = 850U;
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_RELAY) == 100U);

    /* 手动过期后：SOIL_WET 生效 → 关闭灌溉 */
    AdvanceMs(1800000U);
    Tick(&snapshot);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_RELAY) ==
          SERVICE_CONTROL_SOURCE_AUTO);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_RELAY) == 0U);
    return 0;
}

static int test_co2_high_turns_fan_on(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();

    CHECK(ServiceControl_Init() == true);
    snapshot.co2_ppm = 1600U; /* > 1500 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_FAN_PWM) ==
          SERVICE_CONTROL_SOURCE_AUTO);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_FAN_PWM) == 80U);

    snapshot.co2_ppm = 1400U; /* 1300..1500 之间：保持 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_FAN_PWM) == 80U);

    snapshot.co2_ppm = 1200U; /* < 1300：恢复 */
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_FAN_PWM) == 0U);
    return 0;
}

static int test_alarm_buzzer_request(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();

    CHECK(ServiceControl_Init() == true);
    ServiceControl_SetAlarmBuzzerRequest(true);
    Tick(&snapshot);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_BUZZER) ==
          SERVICE_CONTROL_SOURCE_AUTO);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_BUZZER) == 100U);

    ServiceControl_SetAlarmBuzzerRequest(false);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_BUZZER) == 0U);
    return 0;
}

static int test_manual_led_semantics(void)
{
    TerminalSensorSnapshot snapshot = ValidSnapshot();
    uint16_t actual = 0U;

    CHECK(ServiceControl_Init() == true);
    CHECK(ServiceControl_ApplyManualCommand(SERVICE_ACT_LED_STATUS, 100U,
                                            &actual) == true);
    CHECK(actual == 100U);
    Tick(&snapshot);
    CHECK(ServiceControl_GetSource(SERVICE_ACT_LED_STATUS) ==
          SERVICE_CONTROL_SOURCE_MANUAL);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LED_STATUS) == 100U);

    CHECK(ServiceControl_ApplyManualCommand(SERVICE_ACT_LED_STATUS, 0U,
                                            &actual) == true);
    CHECK(actual == 0U);
    Tick(&snapshot);
    CHECK(ServiceControl_GetValue(SERVICE_ACT_LED_STATUS) == 0U);
    return 0;
}

static int test_rejects_invalid_manual(void)
{
    uint16_t actual = 0U;

    CHECK(ServiceControl_Init() == true);
    CHECK(ServiceControl_ApplyManualCommand((ServiceActuator)99U, 10U,
                                            &actual) == false);
    CHECK(ServiceControl_ApplyManualCommand(SERVICE_ACT_LED_STATUS, 101U,
                                            &actual) == false);
    CHECK(ServiceControl_ApplyManualCommand(SERVICE_ACT_LED_STATUS, 10U,
                                            NULL) == false);
    return 0;
}

int main(void)
{
    CHECK(test_init_defaults_off() == 0);
    CHECK(test_light_low_auto_with_confirm() == 0);
    CHECK(test_light_hysteresis_holds_between_thresholds() == 0);
    CHECK(test_manual_override_and_expiry() == 0);
    CHECK(test_safety_overrides_manual() == 0);
    CHECK(test_soil_dry_turns_relay_on() == 0);
    CHECK(test_soil_wet_keeps_relay_off() == 0);
    CHECK(test_co2_high_turns_fan_on() == 0);
    CHECK(test_alarm_buzzer_request() == 0);
    CHECK(test_manual_led_semantics() == 0);
    CHECK(test_rejects_invalid_manual() == 0);
    puts("PASS service_control");
    return 0;
}
