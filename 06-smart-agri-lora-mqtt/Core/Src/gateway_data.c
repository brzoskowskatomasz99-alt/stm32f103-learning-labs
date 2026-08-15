#include "gateway_data.h"

#include "alarm_registry.h"
#include "link_stats.h"
#include "service_hal.h"
#include "terminal_table.h"

#include <stdio.h>
#include <string.h>

static ProtocolLoraTelemetry gateway_last_telemetry;
static bool gateway_telemetry_valid = false;
static int8_t gateway_last_rssi = 0;
static int8_t gateway_last_snr = 0;
static uint8_t gateway_actuator_valid[5];
static uint8_t gateway_actuator_value[5];
static uint8_t gateway_mqtt_connected = 0U;

static uint8_t GatewayData_ActuatorIndex(uint8_t actuator)
{
    switch (actuator)
    {
    case PROTOCOL_LORA_ACTUATOR_LED:
        return 0U;
    case PROTOCOL_LORA_ACTUATOR_BUZZER:
        return 1U;
    case PROTOCOL_LORA_ACTUATOR_RELAY:
        return 2U;
    case PROTOCOL_LORA_ACTUATOR_LIGHT_PWM:
        return 3U;
    case PROTOCOL_LORA_ACTUATOR_FAN_PWM:
        return 4U;
    default:
        return 5U;
    }
}

void GatewayData_Init(void)
{
    memset(&gateway_last_telemetry, 0, sizeof(gateway_last_telemetry));
    gateway_telemetry_valid = false;
    gateway_last_rssi = 0;
    gateway_last_snr = 0;
    memset(gateway_actuator_valid, 0, sizeof(gateway_actuator_valid));
    memset(gateway_actuator_value, 0, sizeof(gateway_actuator_value));
    gateway_mqtt_connected = 0U;
    printf("[GWDATA] INIT OK\r\n");
}

void GatewayData_NoteTelemetry(const ProtocolLoraFrame *frame,
                               int8_t rssi_dbm,
                               int8_t snr_db)
{
    ProtocolLoraTelemetry telemetry;

    if ((frame == NULL) ||
        (ProtocolLora_GetTelemetryPayload(frame, &telemetry) !=
         PROTOCOL_LORA_OK))
    {
        return;
    }
    gateway_last_telemetry = telemetry;
    gateway_telemetry_valid = true;
    gateway_last_rssi = rssi_dbm;
    gateway_last_snr = snr_db;
}

void GatewayData_NoteAck(uint8_t actuator, const ProtocolLoraAck *ack)
{
    uint8_t index = GatewayData_ActuatorIndex(actuator);

    if ((index >= 5U) || (ack == NULL))
    {
        return;
    }
    gateway_actuator_valid[index] = 1U;
    gateway_actuator_value[index] =
        (ack->actual_value > 100U) ? 100U : (uint8_t)ack->actual_value;
}

void GatewayData_NoteMqttState(uint8_t connected)
{
    gateway_mqtt_connected = connected;
}

void GatewayData_BuildUiData(UiOledTelemetry *telemetry,
                             UiOledStatus *status)
{
    uint8_t index;
    uint8_t count;
    uint8_t online_count = 0U;
    uint32_t now = ServiceHal_GetTickMs();

    if (telemetry != NULL)
    {
        telemetry->valid = gateway_telemetry_valid;
        telemetry->temperature_x10 = gateway_last_telemetry.temperature_x10;
        telemetry->humidity_x10 = gateway_last_telemetry.humidity_x10;
        telemetry->co2_ppm = gateway_last_telemetry.co2_ppm;
        telemetry->lux = gateway_last_telemetry.lux;
        telemetry->soil_x10 = gateway_last_telemetry.soil_x10;
        telemetry->rssi_dbm = gateway_last_rssi;
        telemetry->snr_db = gateway_last_snr;
    }
    if (status != NULL)
    {
        status->mqtt_connected = gateway_mqtt_connected;
        status->link_rate_percent = LinkStats_GetSuccessRatePercent();
        count = TerminalTable_GetCount();
        status->terminal_count = count;
        for (index = 0U; index < count; ++index)
        {
            uint16_t id;
            bool online;

            if (TerminalTable_GetEntry(index, &id, &online, now) && online)
            {
                ++online_count;
            }
        }
        status->terminal_online_count = online_count;
        memcpy(status->actuator_valid, gateway_actuator_valid,
               sizeof(gateway_actuator_valid));
        memcpy(status->actuator_value, gateway_actuator_value,
               sizeof(gateway_actuator_value));
    }
}
