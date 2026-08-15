/* ui_oled 主机单元测试：桩实现 UiHal_* 与时钟。 */
#include "ui_oled.h"

#include "alarm_registry.h"
#include "bridge_mqtt.h"
#include "gateway_config.h"
#include "protocol_lora.h"
#include "service_hal.h"
#include "ui_oled_hal.h"

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
static char g_lines[4][32];
static int g_power = 0;
static int g_key1 = 0;
static int g_key2 = 0;
static int g_clear_count = 0;

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

void UiHal_Init(void)
{
    g_power = 1;
}

void UiHal_Clear(void)
{
    memset(g_lines, 0, sizeof(g_lines));
    ++g_clear_count;
}

void UiHal_DrawText(uint8_t x, uint8_t y, const char *text)
{
    uint8_t line;

    (void)x;
    line = (uint8_t)(y / 16U);
    if (line < 4U)
    {
        (void)snprintf(g_lines[line], sizeof(g_lines[line]), "%s", text);
    }
}

void UiHal_Refresh(void)
{
}

void UiHal_SetPower(bool on)
{
    g_power = on ? 1 : 0;
}

uint8_t UiHal_ReadKey1(void)
{
    return (uint8_t)g_key1;
}

uint8_t UiHal_ReadKey2(void)
{
    return (uint8_t)g_key2;
}

/* ---- 工具 ---- */
static void TestReset(void)
{
    g_now_ms = 0U;
    g_key1 = 0;
    g_key2 = 0;
    g_power = 0;
    g_clear_count = 0;
    memset(g_lines, 0, sizeof(g_lines));
}

static void AdvanceMs(uint32_t ms)
{
    g_now_ms += ms;
}

static void PressKey1(uint32_t hold_ms)
{
    g_key1 = 1;
    UiOled_Process(); /* 注册按下 */
    AdvanceMs(hold_ms);
    UiOled_Process(); /* 长按判定 */
    g_key1 = 0;
    AdvanceMs(30U);
    UiOled_Process(); /* 释放（短按判定） */
}

static void PressKey2(uint32_t hold_ms)
{
    g_key2 = 1;
    UiOled_Process();
    AdvanceMs(hold_ms);
    UiOled_Process();
    g_key2 = 0;
    AdvanceMs(30U);
    UiOled_Process();
}

static int HasLine(const char *needle)
{
    unsigned int i;

    for (i = 0U; i < 4U; ++i)
    {
        if (strstr(g_lines[i], needle) != NULL)
        {
            return 1;
        }
    }
    return 0;
}

static ProtocolLoraFrame MakeAlarmFrame(uint16_t code, uint8_t level,
                                        uint8_t active)
{
    ProtocolLoraFrame frame;
    ProtocolLoraAlarm alarm;

    memset(&frame, 0, sizeof(frame));
    frame.version = PROTOCOL_LORA_VERSION_1;
    frame.type = PROTOCOL_LORA_FRAME_ALARM;
    frame.source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID;
    frame.destination_id = PROTOCOL_LORA_GATEWAY_ID;
    frame.sequence = 1U;
    alarm.alarm_code = code;
    alarm.alarm_level = level;
    alarm.active = active;
    (void)ProtocolLora_SetAlarmPayload(&frame, &alarm);
    return frame;
}

static int test_init_and_overview(void)
{
    UiOledTelemetry telemetry;
    UiOledStatus status;

    memset(&telemetry, 0, sizeof(telemetry));
    memset(&status, 0, sizeof(status));
    status.mqtt_connected = 1U;
    status.link_rate_percent = 98U;
    status.terminal_count = 1U;
    status.terminal_online_count = 1U;

    TestReset();
    CHECK(UiOled_Init() == true);
    CHECK(g_power == 1);

    UiOled_SetData(&telemetry, &status);
    AdvanceMs(500U);
    UiOled_Process(); /* 首次渲染在首帧刷新时发生 */
    CHECK(HasLine("粤嵌科技") == 1);
    CHECK(HasLine("MQTT:OK") == 1);
    CHECK(HasLine("TERM:1/1") == 1);
    CHECK(HasLine("ALARM:0") == 1);
    return 0;
}

static int test_sw1_short_next_page(void)
{
    UiOledTelemetry telemetry;
    UiOledStatus status;

    memset(&telemetry, 0, sizeof(telemetry));
    memset(&status, 0, sizeof(status));

    TestReset();
    CHECK(UiOled_Init() == true);
    UiOled_SetData(&telemetry, &status);
    PressKey1(30U); /* 短按 → 下一页（P1 环境，无遥测） */
    AdvanceMs(500U);
    UiOled_Process();
    CHECK(HasLine("NO DATA") == 1);
    return 0;
}

static int test_sw1_long_toggles_rotate(void)
{
    UiOledTelemetry telemetry;
    UiOledStatus status;

    memset(&telemetry, 0, sizeof(telemetry));
    memset(&status, 0, sizeof(status));

    TestReset();
    CHECK(UiOled_Init() == true);
    UiOled_SetData(&telemetry, &status);
    PressKey1(1600U); /* 长按 → 关闭轮显 */
    AdvanceMs(500U);
    UiOled_Process();
    CHECK(HasLine("粤嵌科技") == 1);

    AdvanceMs(GATEWAY_OLED_ROTATE_MS + 100U); /* 超过轮换周期 */
    UiOled_Process();
    AdvanceMs(500U);
    UiOled_Process();
    CHECK(HasLine("粤嵌科技") == 1); /* 轮显关闭：仍在 P0 */
    return 0;
}

static int test_alarm_forces_page_and_browse(void)
{
    ProtocolLoraFrame alarm;
    UiOledTelemetry telemetry;
    UiOledStatus status;

    memset(&telemetry, 0, sizeof(telemetry));
    memset(&status, 0, sizeof(status));

    TestReset();
    CHECK(UiOled_Init() == true);
    CHECK(AlarmRegistry_Init() == true);
    alarm = MakeAlarmFrame(4U, 2U, 1U);
    AlarmRegistry_OnAlarmFrame(&alarm);
    UiOled_SetData(&telemetry, &status);
    AdvanceMs(500U);
    UiOled_Process();
    CHECK(HasLine("CODE:CO2_HIGH") == 1);
    CHECK(HasLine("LEVEL:2") == 1);

    PressKey2(30U); /* 浏览下一条（仅 1 条） */
    AdvanceMs(500U);
    UiOled_Process();
    CHECK(HasLine("ALM:1/1") == 1);

    alarm = MakeAlarmFrame(4U, 2U, 0U); /* 恢复 */
    AlarmRegistry_OnAlarmFrame(&alarm);
    UiOled_SetData(&telemetry, &status);
    AdvanceMs(500U);
    UiOled_Process();
    CHECK(HasLine("粤嵌科技") == 1); /* 报警解除回到正常页 */
    return 0;
}

static int test_sleep_and_wake(void)
{
    UiOledTelemetry telemetry;
    UiOledStatus status;

    memset(&telemetry, 0, sizeof(telemetry));
    memset(&status, 0, sizeof(status));

    TestReset();
    CHECK(UiOled_Init() == true);
    UiOled_SetData(&telemetry, &status);
    AdvanceMs(GATEWAY_OLED_SLEEP_TIMEOUT_MS + 100U);
    UiOled_Process();
    CHECK(g_power == 0); /* 息屏 */

    PressKey1(30U); /* 任意按键唤醒，且不切页 */
    CHECK(g_power == 1);
    AdvanceMs(500U);
    UiOled_Process();
    CHECK(HasLine("粤嵌科技") == 1);
    return 0;
}

static int test_sw2_long_silence_request(void)
{
    UiOledTelemetry telemetry;
    UiOledStatus status;

    memset(&telemetry, 0, sizeof(telemetry));
    memset(&status, 0, sizeof(status));

    TestReset();
    CHECK(UiOled_Init() == true);
    UiOled_SetData(&telemetry, &status);
    PressKey2(1600U);
    CHECK(UiOled_GetSilenceRequest() == true);
    CHECK(UiOled_GetSilenceRequest() == false); /* 一次性 */
    return 0;
}

int main(void)
{
    CHECK(test_init_and_overview() == 0);
    CHECK(test_sw1_short_next_page() == 0);
    CHECK(test_sw1_long_toggles_rotate() == 0);
    CHECK(test_alarm_forces_page_and_browse() == 0);
    CHECK(test_sleep_and_wake() == 0);
    CHECK(test_sw2_long_silence_request() == 0);
    puts("PASS ui_oled");
    return 0;
}
