#include "bridge_mqtt.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int BridgeMqtt_FormatSignedTenths(int16_t value,
                                         char *output,
                                         size_t output_capacity)
{
    int32_t magnitude = value;

    if (magnitude < 0)
    {
        magnitude = -magnitude;
        return snprintf(output, output_capacity, "-%ld.%ld",
                        (long)(magnitude / 10), (long)(magnitude % 10));
    }
    return snprintf(output, output_capacity, "%ld.%ld",
                    (long)(magnitude / 10), (long)(magnitude % 10));
}

static int BridgeMqtt_FormatUnsignedTenths(uint16_t value,
                                           char *output,
                                           size_t output_capacity)
{
    return snprintf(output, output_capacity, "%u.%u",
                    (unsigned int)(value / 10U),
                    (unsigned int)(value % 10U));
}

static BridgeMqttStatus BridgeMqtt_GetTelemetry(
    const ProtocolLoraFrame *frame,
    ProtocolLoraTelemetry *telemetry)
{
    if ((frame == NULL) || (telemetry == NULL))
    {
        return BRIDGE_MQTT_ERROR_ARGUMENT;
    }
    if ((frame->version != PROTOCOL_LORA_VERSION_1) ||
        (frame->type != PROTOCOL_LORA_FRAME_TELEMETRY))
    {
        return BRIDGE_MQTT_ERROR_FRAME;
    }
    if (frame->source_id < PROTOCOL_LORA_FIRST_TERMINAL_ID)
    {
        return BRIDGE_MQTT_ERROR_SOURCE;
    }
    if (frame->destination_id != PROTOCOL_LORA_GATEWAY_ID)
    {
        return BRIDGE_MQTT_ERROR_DESTINATION;
    }
    if (ProtocolLora_GetTelemetryPayload(frame, telemetry) != PROTOCOL_LORA_OK)
    {
        return BRIDGE_MQTT_ERROR_PAYLOAD;
    }
    return BRIDGE_MQTT_OK;
}

BridgeMqttStatus BridgeMqtt_FormatTelemetryJson(
    const ProtocolLoraFrame *frame,
    int8_t rssi_dbm,
    char *output,
    size_t output_capacity,
    size_t *output_length)
{
    ProtocolLoraTelemetry telemetry;
    BridgeMqttStatus status;
    char temperature[16];
    char humidity[16];
    char soil[16];
    int temperature_length;
    int humidity_length;
    int soil_length;
    int json_length;

    if ((frame == NULL) || (output == NULL) || (output_length == NULL) ||
        (output_capacity == 0U))
    {
        return BRIDGE_MQTT_ERROR_ARGUMENT;
    }
    output[0] = '\0';
    *output_length = 0U;

    status = BridgeMqtt_GetTelemetry(frame, &telemetry);
    if (status != BRIDGE_MQTT_OK)
    {
        return status;
    }

    temperature_length = BridgeMqtt_FormatSignedTenths(
        telemetry.temperature_x10, temperature, sizeof(temperature));
    humidity_length = BridgeMqtt_FormatUnsignedTenths(
        telemetry.humidity_x10, humidity, sizeof(humidity));
    soil_length = BridgeMqtt_FormatUnsignedTenths(
        telemetry.soil_x10, soil, sizeof(soil));
    if ((temperature_length < 0) ||
        ((size_t)temperature_length >= sizeof(temperature)) ||
        (humidity_length < 0) || ((size_t)humidity_length >= sizeof(humidity)) ||
        (soil_length < 0) || ((size_t)soil_length >= sizeof(soil)))
    {
        return BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL;
    }

    json_length = snprintf(
        output,
        output_capacity,
        "{\"dev\":\"node-%02u\",\"seq\":%u,\"temp\":%s,\"humi\":%s,"
        "\"co2\":%u,\"lux\":%u,\"soil\":%s,\"rssi\":%d}",
        (unsigned int)frame->source_id,
        (unsigned int)frame->sequence,
        temperature,
        humidity,
        (unsigned int)telemetry.co2_ppm,
        (unsigned int)telemetry.lux,
        soil,
        (int)rssi_dbm);
    if ((json_length < 0) || ((size_t)json_length >= output_capacity))
    {
        output[0] = '\0';
        return BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL;
    }

    *output_length = (size_t)json_length;
    return BRIDGE_MQTT_OK;
}

BridgeMqttStatus BridgeMqtt_FormatBemfaSensor(
    const ProtocolLoraFrame *frame,
    char *output,
    size_t output_capacity,
    size_t *output_length)
{
    ProtocolLoraTelemetry telemetry;
    BridgeMqttStatus status;
    char temperature[16];
    char humidity[16];
    int temperature_length;
    int humidity_length;
    int sensor_length;

    if ((frame == NULL) || (output == NULL) || (output_length == NULL) ||
        (output_capacity == 0U))
    {
        return BRIDGE_MQTT_ERROR_ARGUMENT;
    }
    output[0] = '\0';
    *output_length = 0U;

    status = BridgeMqtt_GetTelemetry(frame, &telemetry);
    if (status != BRIDGE_MQTT_OK)
    {
        return status;
    }

    temperature_length = BridgeMqtt_FormatSignedTenths(
        telemetry.temperature_x10, temperature, sizeof(temperature));
    humidity_length = BridgeMqtt_FormatUnsignedTenths(
        telemetry.humidity_x10, humidity, sizeof(humidity));
    if ((temperature_length < 0) ||
        ((size_t)temperature_length >= sizeof(temperature)) ||
        (humidity_length < 0) || ((size_t)humidity_length >= sizeof(humidity)))
    {
        return BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL;
    }

    sensor_length = snprintf(output, output_capacity, "#%s#%s",
                             temperature, humidity);
    if ((sensor_length < 0) || ((size_t)sensor_length >= output_capacity))
    {
        output[0] = '\0';
        return BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL;
    }

    *output_length = (size_t)sensor_length;
    return BRIDGE_MQTT_OK;
}

static int BridgeMqtt_ReadUnsignedField(const char *json,
                                        const char *name,
                                        unsigned long *value)
{
    const char *field;
    char *end;

    field = strstr(json, name);
    if (field == NULL)
    {
        return 0;
    }
    field += strlen(name);
    *value = strtoul(field, &end, 10);
    return end != field;
}

static int BridgeMqtt_MatchActuator(const char *json, uint8_t *actuator)
{
    if (strstr(json, "\"act\":\"led\"") != NULL)
    {
        *actuator = PROTOCOL_LORA_ACTUATOR_LED;
    }
    else if (strstr(json, "\"act\":\"buzzer\"") != NULL)
    {
        *actuator = PROTOCOL_LORA_ACTUATOR_BUZZER;
    }
    else if (strstr(json, "\"act\":\"relay\"") != NULL)
    {
        *actuator = PROTOCOL_LORA_ACTUATOR_RELAY;
    }
    else if (strstr(json, "\"act\":\"light\"") != NULL)
    {
        *actuator = PROTOCOL_LORA_ACTUATOR_LIGHT_PWM;
    }
    else if (strstr(json, "\"act\":\"fan\"") != NULL)
    {
        *actuator = PROTOCOL_LORA_ACTUATOR_FAN_PWM;
    }
    else
    {
        return 0;
    }
    return 1;
}

BridgeMqttStatus BridgeMqtt_ParseCommandJson(
    const char *json,
    ProtocolLoraFrame *frame,
    uint16_t *command_id)
{
    unsigned long raw_id = 0UL;
    unsigned long value = 0UL;
    uint8_t actuator = 0U;
    uint16_t id = 0U;
    ProtocolLoraCommand command;

    if (command_id != NULL)
    {
        *command_id = 0U;
    }
    if ((json == NULL) || (frame == NULL))
    {
        return BRIDGE_MQTT_ERROR_ARGUMENT;
    }

    /* id 独立提取：非法命令也要尽力拿到 id 以发布错误 ACK */
    if (BridgeMqtt_ReadUnsignedField(json, "\"id\":", &raw_id) &&
        (raw_id > 0UL) && (raw_id <= UINT16_MAX))
    {
        id = (uint16_t)raw_id;
    }
    if (command_id != NULL)
    {
        *command_id = id;
    }

    if ((id == 0U) ||
        (strstr(json, "\"dev\":\"node-02\"") == NULL) ||
        (strstr(json, "\"mode\":\"manual\"") == NULL) ||
        !BridgeMqtt_MatchActuator(json, &actuator) ||
        !BridgeMqtt_ReadUnsignedField(json, "\"value\":", &value) ||
        (value > 100UL))
    {
        return BRIDGE_MQTT_ERROR_PAYLOAD;
    }

    memset(frame, 0, sizeof(*frame));
    frame->version = PROTOCOL_LORA_VERSION_1;
    frame->type = PROTOCOL_LORA_FRAME_COMMAND;
    frame->source_id = PROTOCOL_LORA_GATEWAY_ID;
    frame->destination_id = PROTOCOL_LORA_FIRST_TERMINAL_ID;
    frame->sequence = (uint8_t)id;
    command.command_id = id;
    command.actuator = actuator;
    command.action = (value == 0UL) ? PROTOCOL_LORA_ACTION_OFF
                                    : PROTOCOL_LORA_ACTION_SET;
    command.value = (uint16_t)value;
    command.mode = PROTOCOL_LORA_MODE_MANUAL;
    if (ProtocolLora_SetCommandPayload(frame, &command) != PROTOCOL_LORA_OK)
    {
        return BRIDGE_MQTT_ERROR_PAYLOAD;
    }
    return BRIDGE_MQTT_OK;
}

BridgeMqttStatus BridgeMqtt_FormatAckJsonFromAck(
    const ProtocolLoraAck *ack,
    char *output,
    size_t output_capacity,
    size_t *output_length)
{
    int length;

    if ((ack == NULL) || (output == NULL) || (output_length == NULL) ||
        (output_capacity == 0U))
    {
        return BRIDGE_MQTT_ERROR_ARGUMENT;
    }
    output[0] = '\0';
    *output_length = 0U;

    length = snprintf(output, output_capacity,
                      "{\"id\":%u,\"dev\":\"node-02\",\"result\":\"%s\",\"actual\":%u}",
                      (unsigned int)ack->command_id,
                      (ack->result == PROTOCOL_LORA_ACK_OK) ? "ok" : "error",
                      (unsigned int)ack->actual_value);
    if ((length < 0) || ((size_t)length >= output_capacity))
    {
        output[0] = '\0';
        return BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL;
    }
    *output_length = (size_t)length;
    return BRIDGE_MQTT_OK;
}

BridgeMqttStatus BridgeMqtt_FormatAckJson(
    const ProtocolLoraFrame *frame,
    char *output,
    size_t output_capacity,
    size_t *output_length)
{
    ProtocolLoraAck ack;

    if (output_length != NULL)
    {
        *output_length = 0U;
    }
    if ((frame == NULL) || (output == NULL) || (output_length == NULL) ||
        (output_capacity == 0U))
    {
        return BRIDGE_MQTT_ERROR_ARGUMENT;
    }
    output[0] = '\0';
    if ((frame->version != PROTOCOL_LORA_VERSION_1) ||
        (frame->type != PROTOCOL_LORA_FRAME_ACK) ||
        (frame->source_id != PROTOCOL_LORA_FIRST_TERMINAL_ID) ||
        (frame->destination_id != PROTOCOL_LORA_GATEWAY_ID) ||
        (ProtocolLora_GetAckPayload(frame, &ack) != PROTOCOL_LORA_OK))
    {
        return BRIDGE_MQTT_ERROR_FRAME;
    }
    return BridgeMqtt_FormatAckJsonFromAck(&ack, output, output_capacity,
                                           output_length);
}

BridgeMqttStatus BridgeMqtt_FormatErrorAckJson(
    uint16_t command_id,
    char *output,
    size_t output_capacity,
    size_t *output_length)
{
    int length;

    if ((output == NULL) || (output_length == NULL) || (output_capacity == 0U))
    {
        return BRIDGE_MQTT_ERROR_ARGUMENT;
    }
    output[0] = '\0';
    *output_length = 0U;
    if (command_id == 0U)
    {
        return BRIDGE_MQTT_ERROR_ARGUMENT;
    }

    length = snprintf(output, output_capacity,
                      "{\"id\":%u,\"dev\":\"node-02\",\"result\":\"error\"}",
                      (unsigned int)command_id);
    if ((length < 0) || ((size_t)length >= output_capacity))
    {
        output[0] = '\0';
        return BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL;
    }
    *output_length = (size_t)length;
    return BRIDGE_MQTT_OK;
}

const char *BridgeMqtt_AlarmCodeString(uint16_t code)
{
    switch (code)
    {
    case 1U:
        return "LIGHT_LOW";
    case 2U:
        return "SOIL_WET";
    case 3U:
        return "SOIL_DRY";
    case 4U:
        return "CO2_HIGH";
    case 5U:
        return "TEMP_ALARM";
    case 6U:
        return "HUMI_ALARM";
    case 7U:
        return "SENSOR_FAULT";
    default:
        return "UNKNOWN";
    }
}

BridgeMqttStatus BridgeMqtt_FormatAlarmJson(
    const ProtocolLoraFrame *frame,
    char *output,
    size_t output_capacity,
    size_t *output_length)
{
    ProtocolLoraAlarm alarm;
    int length;

    if ((frame == NULL) || (output == NULL) || (output_length == NULL) ||
        (output_capacity == 0U))
    {
        return BRIDGE_MQTT_ERROR_ARGUMENT;
    }
    output[0] = '\0';
    *output_length = 0U;
    if ((frame->version != PROTOCOL_LORA_VERSION_1) ||
        (frame->type != PROTOCOL_LORA_FRAME_ALARM) ||
        (frame->source_id < PROTOCOL_LORA_FIRST_TERMINAL_ID) ||
        (frame->destination_id != PROTOCOL_LORA_GATEWAY_ID) ||
        (ProtocolLora_GetAlarmPayload(frame, &alarm) != PROTOCOL_LORA_OK))
    {
        return BRIDGE_MQTT_ERROR_FRAME;
    }
    /* 告警码只允许 1..7：拒绝对端固件/噪声产生的垃圾码 */
    if ((alarm.alarm_code < 1U) || (alarm.alarm_code > 7U) ||
        (alarm.alarm_level == 0U))
    {
        return BRIDGE_MQTT_ERROR_PAYLOAD;
    }

    length = snprintf(output, output_capacity,
                      "{\"dev\":\"node-%02u\",\"code\":\"%s\",\"active\":%s,\"level\":%u}",
                      (unsigned int)frame->source_id,
                      BridgeMqtt_AlarmCodeString(alarm.alarm_code),
                      (alarm.active != 0U) ? "true" : "false",
                      (unsigned int)alarm.alarm_level);
    if ((length < 0) || ((size_t)length >= output_capacity))
    {
        output[0] = '\0';
        return BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL;
    }
    *output_length = (size_t)length;
    return BRIDGE_MQTT_OK;
}

BridgeMqttStatus BridgeMqtt_FormatStatusJson(
    uint8_t mqtt_connected,
    uint8_t link_rate_percent,
    const uint16_t *terminal_ids,
    const bool *terminal_online,
    uint8_t terminal_count,
    char *output,
    size_t output_capacity,
    size_t *output_length)
{
    size_t used;
    int length;
    uint8_t index;

    if ((output == NULL) || (output_length == NULL) || (output_capacity == 0U) ||
        ((terminal_count > 0U) &&
         ((terminal_ids == NULL) || (terminal_online == NULL))))
    {
        return BRIDGE_MQTT_ERROR_ARGUMENT;
    }
    output[0] = '\0';
    *output_length = 0U;

    length = snprintf(output, output_capacity,
                      "{\"gw\":\"node-01\",\"mqtt\":%s,\"link_rate\":%u,"
                      "\"terminals\":[",
                      (mqtt_connected != 0U) ? "true" : "false",
                      (unsigned int)link_rate_percent);
    if ((length < 0) || ((size_t)length >= output_capacity))
    {
        output[0] = '\0';
        return BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL;
    }
    used = (size_t)length;

    for (index = 0U; index < terminal_count; ++index)
    {
        length = snprintf(&output[used], output_capacity - used,
                          "%s{\"dev\":\"node-%02u\",\"online\":%s}",
                          (index == 0U) ? "" : ",",
                          (unsigned int)terminal_ids[index],
                          (terminal_online[index] != 0U) ? "true" : "false");
        if ((length < 0) || ((size_t)length >= (output_capacity - used)))
        {
            output[0] = '\0';
            return BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL;
        }
        used += (size_t)length;
    }

    length = snprintf(&output[used], output_capacity - used, "]}");
    if ((length < 0) || ((size_t)length >= (output_capacity - used)))
    {
        output[0] = '\0';
        return BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL;
    }
    used += (size_t)length;
    *output_length = used;
    return BRIDGE_MQTT_OK;
}
