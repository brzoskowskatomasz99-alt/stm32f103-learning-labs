#include "service_control.h"

#include "terminal_config.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    bool active;
    bool pending;
    uint32_t pending_since_ms;
} ServiceControlRule;

typedef struct
{
    bool active;
    uint32_t expires_ms;
    uint8_t value;
} ServiceControlManual;

static ServiceControlRule control_light_low;
static ServiceControlRule control_soil_wet;
static ServiceControlRule control_soil_dry;
static ServiceControlRule control_co2_high;
static ServiceControlManual control_manual[SERVICE_ACT_COUNT];
static bool control_alarm_buzzer_request = false;
static uint32_t control_tick_ms = 0U;
static uint8_t control_applied_value[SERVICE_ACT_COUNT];
static ServiceControlSource control_applied_source[SERVICE_ACT_COUNT];

static const char *ControlSourceName(ServiceControlSource source)
{
    switch (source)
    {
    case SERVICE_CONTROL_SOURCE_AUTO:
        return "AUTO";
    case SERVICE_CONTROL_SOURCE_MANUAL:
        return "MANUAL";
    case SERVICE_CONTROL_SOURCE_SAFE:
        return "SAFE";
    default:
        return "OFF";
    }
}

static void ControlUpdateRule(ServiceControlRule *rule,
                              bool set_condition,
                              bool clear_condition,
                              uint32_t now,
                              const char *name)
{
    bool desired = rule->active ? !clear_condition : set_condition;

    if (desired != rule->pending)
    {
        rule->pending = desired;
        rule->pending_since_ms = now;
    }
    if ((rule->pending != rule->active) &&
        ((now - rule->pending_since_ms) >= TERMINAL_RULE_CONFIRM_MS))
    {
        rule->active = rule->pending;
        printf("[CTRL] RULE=%s %s\r\n", name,
               rule->active ? "ACTIVE" : "CLEAR");
    }
}

static bool ControlManualActive(ServiceActuator actuator, uint32_t now)
{
    ServiceControlManual *manual = &control_manual[actuator];

    if (!manual->active)
    {
        return false;
    }
    if (((int32_t)(now - manual->expires_ms)) >= 0)
    {
        manual->active = false;
        printf("[CTRL] MANUAL EXPIRE ACT=%d\r\n", (int)actuator);
        return false;
    }
    return true;
}

static void ControlApply(ServiceActuator actuator,
                         ServiceControlSource source,
                         uint8_t value)
{
    if ((control_applied_source[actuator] == source) &&
        (control_applied_value[actuator] == value))
    {
        return;
    }
    ServiceHal_ActuatorWrite(actuator, value);
    control_applied_source[actuator] = source;
    control_applied_value[actuator] = value;
    printf("[CTRL] ACT=%d SRC=%s VALUE=%u\r\n", (int)actuator,
           ControlSourceName(source), (unsigned int)value);
}

bool ServiceControl_Init(void)
{
    ServiceActuator index;

    memset(&control_light_low, 0, sizeof(control_light_low));
    memset(&control_soil_wet, 0, sizeof(control_soil_wet));
    memset(&control_soil_dry, 0, sizeof(control_soil_dry));
    memset(&control_co2_high, 0, sizeof(control_co2_high));
    memset(control_manual, 0, sizeof(control_manual));
    control_alarm_buzzer_request = false;
    control_tick_ms = ServiceHal_GetTickMs();

    for (index = SERVICE_ACT_LED_STATUS; index < SERVICE_ACT_COUNT; ++index)
    {
        control_applied_value[index] = 0xFFU;
        control_applied_source[index] = SERVICE_CONTROL_SOURCE_OFF;
        ServiceHal_ActuatorWrite(index, 0U);
    }
    printf("[CTRL] INIT OK MANUAL_VALIDITY_MS=%lu\r\n",
           (unsigned long)TERMINAL_MANUAL_VALIDITY_MS);
    return true;
}

void ServiceControl_Process(const TerminalSensorSnapshot *snapshot)
{
    uint32_t now;
    uint16_t status;
    bool light_fault;
    bool soil_fault;
    bool co2_fault;
    bool adc_fault;
    bool lux_valid;
    bool soil_valid;
    bool co2_valid;
    ServiceControlSource source_light;
    ServiceControlSource source_relay;
    ServiceControlSource source_fan;
    ServiceControlSource source_led;
    ServiceControlSource source_buzzer;
    uint8_t value_light;
    uint8_t value_relay;
    uint8_t value_fan;
    uint8_t value_led;
    uint8_t value_buzzer;

    if (snapshot == NULL)
    {
        return;
    }
    now = ServiceHal_GetTickMs();
    if (((int32_t)(now - control_tick_ms)) <
        (int32_t)TERMINAL_CONTROL_TICK_MS)
    {
        return;
    }
    control_tick_ms = now;

    status = snapshot->device_status;
    light_fault = (status & TERMINAL_SENSOR_STATUS_LIGHT_FAULT) != 0U;
    soil_fault = (status & TERMINAL_SENSOR_STATUS_SOIL_FAULT) != 0U;
    co2_fault = (status & TERMINAL_SENSOR_STATUS_CO2_FAULT) != 0U;
    adc_fault = (status & TERMINAL_SENSOR_STATUS_ADC_FAULT) != 0U;
    lux_valid = (status & TERMINAL_SENSOR_STATUS_LIGHT_INVALID) == 0U;
    soil_valid = (status & TERMINAL_SENSOR_STATUS_SOIL_INVALID) == 0U;
    co2_valid = (status & TERMINAL_SENSOR_STATUS_CO2_INVALID) == 0U;

    /* ---- 阈值规则（滞回 + 延时确认）---- */
    ControlUpdateRule(&control_light_low,
                      lux_valid &&
                          (snapshot->lux < TERMINAL_LIGHT_LOW_ON_LUX),
                      /* GL5528 查表饱和上限为 350 lux，恢复条件用 >= */
                      lux_valid &&
                          (snapshot->lux >= TERMINAL_LIGHT_LOW_OFF_LUX),
                      now, "LIGHT_LOW");
    ControlUpdateRule(&control_soil_wet,
                      soil_valid &&
                          (snapshot->soil_x10 > TERMINAL_SOIL_WET_ON_X10),
                      soil_valid &&
                          (snapshot->soil_x10 < TERMINAL_SOIL_WET_OFF_X10),
                      now, "SOIL_WET");
    ControlUpdateRule(&control_soil_dry,
                      soil_valid &&
                          (snapshot->soil_x10 < TERMINAL_SOIL_DRY_ON_X10),
                      soil_valid &&
                          (snapshot->soil_x10 > TERMINAL_SOIL_DRY_OFF_X10),
                      now, "SOIL_DRY");
    ControlUpdateRule(&control_co2_high,
                      co2_valid &&
                          (snapshot->co2_ppm > TERMINAL_CO2_HIGH_ON_PPM),
                      co2_valid &&
                          (snapshot->co2_ppm < TERMINAL_CO2_HIGH_OFF_PPM),
                      now, "CO2_HIGH");

    /* ---- 仲裁：P1 安全 > P3 手动 > P2 自动 > 关 ---- */
    if (light_fault || adc_fault)
    {
        source_light = SERVICE_CONTROL_SOURCE_SAFE;
        value_light = 0U;
    }
    else if (ControlManualActive(SERVICE_ACT_LIGHT_PWM, now))
    {
        source_light = SERVICE_CONTROL_SOURCE_MANUAL;
        value_light = control_manual[SERVICE_ACT_LIGHT_PWM].value;
    }
    else if (control_light_low.active)
    {
        source_light = SERVICE_CONTROL_SOURCE_AUTO;
        value_light = (uint8_t)TERMINAL_AUTO_LIGHT_PERCENT;
    }
    else
    {
        source_light = SERVICE_CONTROL_SOURCE_OFF;
        value_light = 0U;
    }

    if (soil_fault || adc_fault)
    {
        source_relay = SERVICE_CONTROL_SOURCE_SAFE;
        value_relay = 0U;
    }
    else if (ControlManualActive(SERVICE_ACT_RELAY, now))
    {
        source_relay = SERVICE_CONTROL_SOURCE_MANUAL;
        value_relay = control_manual[SERVICE_ACT_RELAY].value;
    }
    else if (control_soil_wet.active)
    {
        source_relay = SERVICE_CONTROL_SOURCE_AUTO;
        value_relay = 0U; /* 关闭灌溉 */
    }
    else if (control_soil_dry.active)
    {
        source_relay = SERVICE_CONTROL_SOURCE_AUTO;
        value_relay = 100U; /* 允许开启灌溉 */
    }
    else
    {
        source_relay = SERVICE_CONTROL_SOURCE_OFF;
        value_relay = 0U;
    }

    if (co2_fault)
    {
        source_fan = SERVICE_CONTROL_SOURCE_SAFE;
        value_fan = 0U;
    }
    else if (ControlManualActive(SERVICE_ACT_FAN_PWM, now))
    {
        source_fan = SERVICE_CONTROL_SOURCE_MANUAL;
        value_fan = control_manual[SERVICE_ACT_FAN_PWM].value;
    }
    else if (control_co2_high.active)
    {
        source_fan = SERVICE_CONTROL_SOURCE_AUTO;
        value_fan = (uint8_t)TERMINAL_AUTO_FAN_PERCENT;
    }
    else
    {
        source_fan = SERVICE_CONTROL_SOURCE_OFF;
        value_fan = 0U;
    }

    if (ControlManualActive(SERVICE_ACT_LED_STATUS, now))
    {
        source_led = SERVICE_CONTROL_SOURCE_MANUAL;
        value_led = control_manual[SERVICE_ACT_LED_STATUS].value;
    }
    else
    {
        source_led = SERVICE_CONTROL_SOURCE_OFF;
        value_led = 0U;
    }

    if (ControlManualActive(SERVICE_ACT_BUZZER, now))
    {
        source_buzzer = SERVICE_CONTROL_SOURCE_MANUAL;
        value_buzzer = control_manual[SERVICE_ACT_BUZZER].value;
    }
    else if (control_alarm_buzzer_request)
    {
        source_buzzer = SERVICE_CONTROL_SOURCE_AUTO;
        value_buzzer = 100U;
    }
    else
    {
        source_buzzer = SERVICE_CONTROL_SOURCE_OFF;
        value_buzzer = 0U;
    }

    ControlApply(SERVICE_ACT_LIGHT_PWM, source_light, value_light);
    ControlApply(SERVICE_ACT_RELAY, source_relay, value_relay);
    ControlApply(SERVICE_ACT_FAN_PWM, source_fan, value_fan);
    ControlApply(SERVICE_ACT_LED_STATUS, source_led, value_led);
    ControlApply(SERVICE_ACT_BUZZER, source_buzzer, value_buzzer);
}

ServiceControlSource ServiceControl_GetSource(ServiceActuator actuator)
{
    if (actuator >= SERVICE_ACT_COUNT)
    {
        return SERVICE_CONTROL_SOURCE_OFF;
    }
    return control_applied_source[actuator];
}

uint8_t ServiceControl_GetValue(ServiceActuator actuator)
{
    if (actuator >= SERVICE_ACT_COUNT)
    {
        return 0U;
    }
    return control_applied_value[actuator];
}

bool ServiceControl_ApplyManualCommand(ServiceActuator actuator,
                                       uint8_t value,
                                       uint16_t *actual)
{
    ServiceControlManual *manual;

    if ((actuator >= SERVICE_ACT_COUNT) || (actual == NULL) || (value > 100U))
    {
        return false;
    }
    manual = &control_manual[actuator];
    manual->active = true;
    manual->value = value;
    manual->expires_ms = ServiceHal_GetTickMs() + TERMINAL_MANUAL_VALIDITY_MS;
    *actual = value;
    printf("[CTRL] MANUAL SET ACT=%d VALUE=%u\r\n", (int)actuator,
           (unsigned int)value);
    return true;
}

void ServiceControl_SetAlarmBuzzerRequest(bool on)
{
    control_alarm_buzzer_request = on;
}
