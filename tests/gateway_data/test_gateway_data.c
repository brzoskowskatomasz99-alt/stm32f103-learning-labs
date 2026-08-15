/* gateway_data 主机单元测试：桩实现依赖模块。 */
#include "gateway_data.h"

#include "link_stats.h"
#include "service_hal.h"
#include "terminal_table.h"

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
static uint8_t g_link_rate = 0U;
static uint8_t g_terminal_count = 0U;
static uint8_t g_terminal_online = 0U;

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

uint8_t LinkStats_GetSuccessRatePercent(void)
{
    return g_link_rate;
}

uint16_t LinkStats_GetTotalFrames(void)
{
    return 0U;
}

uint8_t TerminalTable_GetCount(void)
{
    return g_terminal_count;
}

bool TerminalTable_GetEntry(uint8_t index, uint16_t *id, bool *online,
                            uint32_t now)
{
    (void)now;
    if (index >= g_terminal_count)
    {
        return false;
    }
    *id = (uint16_t)(2U + index);
    *online = (index < g_terminal_online) ? true : false;
    return true;
}

/* ---- 工具 ---- */
static ProtocolLoraFrame MakeTelemetryFrame(void)
{
    ProtocolLoraFrame frame;
    ProtocolLoraTelemetry telemetry;

    memset(&frame, 0, sizeof(frame));
    frame.version = PROTOCOL_LORA_VERSION_1;
    frame.type = PROTOCOL_LORA_FRAME_TELEMETRY;
    frame.source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID;
    frame.destination_id = PROTOCOL_LORA_GATEWAY_ID;
    frame.sequence = 7U;
    telemetry.temperature_x10 = 256;
    telemetry.humidity_x10 = 621U;
    telemetry.co2_ppm = 860U;
    telemetry.lux = 420U;
    telemetry.soil_x10 = 485U;
    telemetry.device_status = 0U;
    (void)ProtocolLora_SetTelemetryPayload(&frame, &telemetry);
    return frame;
}

static int test_telemetry_flows_to_ui_data(void)
{
    ProtocolLoraFrame frame = MakeTelemetryFrame();
    UiOledTelemetry telemetry;
    UiOledStatus status;

    GatewayData_Init();
    memset(&telemetry, 0xFF, sizeof(telemetry));
    memset(&status, 0xFF, sizeof(status));
    GatewayData_BuildUiData(&telemetry, &status);
    CHECK(telemetry.valid == false); /* 尚无遥测 */

    GatewayData_NoteTelemetry(&frame, -78, 11);
    GatewayData_NoteMqttState(1U);
    g_link_rate = 98U;
    g_terminal_count = 2U;
    g_terminal_online = 1U;

    GatewayData_BuildUiData(&telemetry, &status);
    CHECK(telemetry.valid == true);
    CHECK(telemetry.temperature_x10 == 256);
    CHECK(telemetry.humidity_x10 == 621U);
    CHECK(telemetry.co2_ppm == 860U);
    CHECK(telemetry.lux == 420U);
    CHECK(telemetry.soil_x10 == 485U);
    CHECK(telemetry.rssi_dbm == -78);
    CHECK(telemetry.snr_db == 11);
    CHECK(status.mqtt_connected == 1U);
    CHECK(status.link_rate_percent == 98U);
    CHECK(status.terminal_count == 2U);
    CHECK(status.terminal_online_count == 1U);
    return 0;
}

static int test_ack_maps_actuator_and_clamps(void)
{
    ProtocolLoraAck ack;
    UiOledTelemetry telemetry;
    UiOledStatus status;

    GatewayData_Init();
    ack.command_id = 1U;
    ack.result = PROTOCOL_LORA_ACK_OK;
    ack.actual_value = 70U;
    ack.device_status = 0U;

    GatewayData_NoteAck(PROTOCOL_LORA_ACTUATOR_LED, &ack);
    GatewayData_NoteAck(PROTOCOL_LORA_ACTUATOR_FAN_PWM, &ack);
    ack.actual_value = 250U; /* 越界 → 钳制 100 */
    GatewayData_NoteAck(PROTOCOL_LORA_ACTUATOR_LIGHT_PWM, &ack);
    GatewayData_NoteAck(99U, &ack); /* 未知执行器 → 忽略 */

    GatewayData_BuildUiData(&telemetry, &status);
    CHECK(status.actuator_valid[0] == 1U);
    CHECK(status.actuator_value[0] == 70U);
    CHECK(status.actuator_valid[4] == 1U);
    CHECK(status.actuator_value[4] == 70U);
    CHECK(status.actuator_valid[3] == 1U);
    CHECK(status.actuator_value[3] == 100U);
    CHECK(status.actuator_valid[2] == 0U); /* 未收到 */
    return 0;
}

int main(void)
{
    CHECK(test_telemetry_flows_to_ui_data() == 0);
    CHECK(test_ack_maps_actuator_and_clamps() == 0);
    puts("PASS gateway_data");
    return 0;
}
