#include "bridge_mqtt.h"

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

static ProtocolLoraFrame make_telemetry_frame(int16_t temperature_x10)
{
    const ProtocolLoraTelemetry telemetry = {
        .temperature_x10 = temperature_x10,
        .humidity_x10 = 621U,
        .co2_ppm = 860U,
        .lux = 420U,
        .soil_x10 = 485U,
        .device_status = 0U,
    };
    ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_TELEMETRY,
        .source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID,
        .destination_id = PROTOCOL_LORA_GATEWAY_ID,
        .sequence = 18U,
    };

    (void)ProtocolLora_SetTelemetryPayload(&frame, &telemetry);
    return frame;
}

static int test_formats_taskbook_example(void)
{
    const char *expected =
        "{\"dev\":\"node-02\",\"seq\":18,\"temp\":25.6,\"humi\":62.1,"
        "\"co2\":860,\"lux\":420,\"soil\":48.5,\"rssi\":-78}";
    ProtocolLoraFrame frame = make_telemetry_frame(256);
    char output[BRIDGE_MQTT_JSON_BUFFER_SIZE];
    size_t output_length = 0U;

    CHECK(BridgeMqtt_FormatTelemetryJson(&frame, -78, output, sizeof(output),
                                         &output_length) == BRIDGE_MQTT_OK);
    CHECK(strcmp(output, expected) == 0);
    CHECK(output_length == strlen(expected));
    return 0;
}

static int test_preserves_negative_fraction(void)
{
    ProtocolLoraFrame frame = make_telemetry_frame(-5);
    char output[BRIDGE_MQTT_JSON_BUFFER_SIZE];
    size_t output_length = 0U;

    CHECK(BridgeMqtt_FormatTelemetryJson(&frame, -90, output, sizeof(output),
                                         &output_length) == BRIDGE_MQTT_OK);
    CHECK(strstr(output, "\"temp\":-0.5") != NULL);
    return 0;
}

static int test_formats_bemfa_sensor_payload(void)
{
    ProtocolLoraFrame frame = make_telemetry_frame(256);
    char output[BRIDGE_MQTT_BEMFA_SENSOR_BUFFER_SIZE];
    size_t output_length = 0U;

    CHECK(BridgeMqtt_FormatBemfaSensor(&frame, output, sizeof(output),
                                       &output_length) == BRIDGE_MQTT_OK);
    CHECK(strcmp(output, "#25.6#62.1") == 0);
    CHECK(output_length == strlen(output));
    return 0;
}

static int test_bemfa_sensor_preserves_negative_fraction(void)
{
    ProtocolLoraFrame frame = make_telemetry_frame(-5);
    char output[BRIDGE_MQTT_BEMFA_SENSOR_BUFFER_SIZE];
    size_t output_length = 0U;

    CHECK(BridgeMqtt_FormatBemfaSensor(&frame, output, sizeof(output),
                                       &output_length) == BRIDGE_MQTT_OK);
    CHECK(strcmp(output, "#-0.5#62.1") == 0);
    return 0;
}

static int test_bemfa_sensor_requires_space_for_terminator(void)
{
    ProtocolLoraFrame frame = make_telemetry_frame(256);
    char full[BRIDGE_MQTT_BEMFA_SENSOR_BUFFER_SIZE];
    char exact[BRIDGE_MQTT_BEMFA_SENSOR_BUFFER_SIZE];
    size_t full_length = 0U;
    size_t output_length = 0U;

    CHECK(BridgeMqtt_FormatBemfaSensor(&frame, full, sizeof(full),
                                       &full_length) == BRIDGE_MQTT_OK);
    CHECK(BridgeMqtt_FormatBemfaSensor(&frame, exact, full_length + 1U,
                                       &output_length) == BRIDGE_MQTT_OK);
    CHECK(BridgeMqtt_FormatBemfaSensor(&frame, exact, full_length,
                                       &output_length) ==
          BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL);
    CHECK(output_length == 0U);
    CHECK(exact[0] == '\0');
    return 0;
}

static int test_rejects_wrong_frame_identity(void)
{
    ProtocolLoraFrame frame = make_telemetry_frame(256);
    char output[BRIDGE_MQTT_JSON_BUFFER_SIZE];
    size_t output_length = 99U;

    frame.type = PROTOCOL_LORA_FRAME_COMMAND;
    CHECK(BridgeMqtt_FormatTelemetryJson(&frame, -78, output, sizeof(output),
                                         &output_length) == BRIDGE_MQTT_ERROR_FRAME);

    frame = make_telemetry_frame(256);
    frame.source_id = PROTOCOL_LORA_GATEWAY_ID;
    CHECK(BridgeMqtt_FormatTelemetryJson(&frame, -78, output, sizeof(output),
                                         &output_length) == BRIDGE_MQTT_ERROR_SOURCE);

    frame = make_telemetry_frame(256);
    frame.destination_id = PROTOCOL_LORA_FIRST_TERMINAL_ID;
    CHECK(BridgeMqtt_FormatTelemetryJson(&frame, -78, output, sizeof(output),
                                         &output_length) ==
          BRIDGE_MQTT_ERROR_DESTINATION);
    return 0;
}

static int test_rejects_bad_payload_length(void)
{
    ProtocolLoraFrame frame = make_telemetry_frame(256);
    char output[BRIDGE_MQTT_JSON_BUFFER_SIZE];
    size_t output_length = 0U;

    frame.payload_length--;
    CHECK(BridgeMqtt_FormatTelemetryJson(&frame, -78, output, sizeof(output),
                                         &output_length) == BRIDGE_MQTT_ERROR_PAYLOAD);
    return 0;
}

static int test_requires_space_for_terminator(void)
{
    ProtocolLoraFrame frame = make_telemetry_frame(256);
    char full[BRIDGE_MQTT_JSON_BUFFER_SIZE];
    char exact[BRIDGE_MQTT_JSON_BUFFER_SIZE];
    size_t full_length = 0U;
    size_t output_length = 0U;

    CHECK(BridgeMqtt_FormatTelemetryJson(&frame, -78, full, sizeof(full),
                                         &full_length) == BRIDGE_MQTT_OK);
    CHECK(BridgeMqtt_FormatTelemetryJson(&frame, -78, exact, full_length + 1U,
                                         &output_length) == BRIDGE_MQTT_OK);
    CHECK(BridgeMqtt_FormatTelemetryJson(&frame, -78, exact, full_length,
                                         &output_length) ==
          BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL);
    CHECK(output_length == 0U);
    CHECK(exact[0] == '\0');
    return 0;
}

static int test_maximum_field_values_fit_configured_buffer(void)
{
    const ProtocolLoraTelemetry telemetry = {
        .temperature_x10 = INT16_MIN,
        .humidity_x10 = UINT16_MAX,
        .co2_ppm = UINT16_MAX,
        .lux = UINT16_MAX,
        .soil_x10 = UINT16_MAX,
        .device_status = UINT16_MAX,
    };
    ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_TELEMETRY,
        .source_id = UINT16_MAX,
        .destination_id = PROTOCOL_LORA_GATEWAY_ID,
        .sequence = UINT8_MAX,
    };
    char output[BRIDGE_MQTT_JSON_BUFFER_SIZE];
    size_t output_length = 0U;

    CHECK(ProtocolLora_SetTelemetryPayload(&frame, &telemetry) == PROTOCOL_LORA_OK);
    CHECK(BridgeMqtt_FormatTelemetryJson(&frame, INT8_MIN, output, sizeof(output),
                                         &output_length) == BRIDGE_MQTT_OK);
    CHECK(output_length < sizeof(output));
    CHECK(strstr(output, "\"dev\":\"node-65535\"") != NULL);
    CHECK(strstr(output, "\"temp\":-3276.8") != NULL);
    CHECK(strstr(output, "\"rssi\":-128") != NULL);
    return 0;
}

static int test_parses_led_command(void)
{
    ProtocolLoraFrame frame;
    ProtocolLoraCommand command;
    uint16_t command_id = 0U;

    CHECK(BridgeMqtt_ParseCommandJson(
              "{\"id\":1001,\"dev\":\"node-02\",\"act\":\"led\",\"value\":70,\"mode\":\"manual\"}",
              &frame, &command_id) == BRIDGE_MQTT_OK);
    CHECK(command_id == 1001U);
    CHECK(frame.type == PROTOCOL_LORA_FRAME_COMMAND);
    CHECK(frame.source_id == PROTOCOL_LORA_GATEWAY_ID);
    CHECK(frame.destination_id == PROTOCOL_LORA_FIRST_TERMINAL_ID);
    CHECK(ProtocolLora_GetCommandPayload(&frame, &command) == PROTOCOL_LORA_OK);
    CHECK(command.command_id == 1001U);
    CHECK(command.actuator == PROTOCOL_LORA_ACTUATOR_LED);
    CHECK(command.action == PROTOCOL_LORA_ACTION_SET);
    CHECK(command.value == 70U);
    CHECK(command.mode == PROTOCOL_LORA_MODE_MANUAL);
    return 0;
}

static int test_parses_led_off_command(void)
{
    ProtocolLoraFrame frame;
    ProtocolLoraCommand command;
    uint16_t command_id = 0U;

    CHECK(BridgeMqtt_ParseCommandJson(
              "{\"id\":1002,\"dev\":\"node-02\",\"act\":\"led\",\"value\":0,\"mode\":\"manual\"}",
              &frame, &command_id) == BRIDGE_MQTT_OK);
    CHECK(command_id == 1002U);
    CHECK(ProtocolLora_GetCommandPayload(&frame, &command) == PROTOCOL_LORA_OK);
    CHECK(command.actuator == PROTOCOL_LORA_ACTUATOR_LED);
    CHECK(command.action == PROTOCOL_LORA_ACTION_OFF);
    CHECK(command.value == 0U);
    return 0;
}

static int test_parses_all_five_actuators(void)
{
    const char *acts[] = {"led", "buzzer", "relay", "light", "fan"};
    const uint8_t expect[] = {
        PROTOCOL_LORA_ACTUATOR_LED, PROTOCOL_LORA_ACTUATOR_BUZZER,
        PROTOCOL_LORA_ACTUATOR_RELAY, PROTOCOL_LORA_ACTUATOR_LIGHT_PWM,
        PROTOCOL_LORA_ACTUATOR_FAN_PWM};
    ProtocolLoraFrame frame;
    ProtocolLoraCommand command;
    char json[128];
    uint16_t command_id = 0U;
    unsigned int i;

    for (i = 0U; i < 5U; ++i)
    {
        snprintf(json, sizeof(json),
                 "{\"id\":200%u,\"dev\":\"node-02\",\"act\":\"%s\",\"value\":80,\"mode\":\"manual\"}",
                 i, acts[i]);
        CHECK(BridgeMqtt_ParseCommandJson(json, &frame, &command_id) ==
              BRIDGE_MQTT_OK);
        CHECK(command_id == 2000U + i);
        CHECK(ProtocolLora_GetCommandPayload(&frame, &command) ==
              PROTOCOL_LORA_OK);
        CHECK(command.actuator == expect[i]);
        CHECK(command.action == PROTOCOL_LORA_ACTION_SET);
        CHECK(command.value == 80U);
    }
    return 0;
}

static int test_rejects_invalid_command_and_still_gets_id(void)
{
    ProtocolLoraFrame frame;
    uint16_t command_id = 0U;

    CHECK(BridgeMqtt_ParseCommandJson(
              "{\"id\":1001,\"dev\":\"node-03\",\"act\":\"led\",\"value\":70,\"mode\":\"manual\"}",
              &frame, &command_id) == BRIDGE_MQTT_ERROR_PAYLOAD);
    CHECK(command_id == 1001U); /* 非法命令仍提取 id 供错误 ACK */

    command_id = 0U;
    CHECK(BridgeMqtt_ParseCommandJson(
              "{\"id\":1001,\"dev\":\"node-02\",\"act\":\"led\",\"value\":101,\"mode\":\"manual\"}",
              &frame, &command_id) == BRIDGE_MQTT_ERROR_PAYLOAD);
    CHECK(command_id == 1001U);

    command_id = 0U;
    CHECK(BridgeMqtt_ParseCommandJson(
              "{\"id\":1001,\"dev\":\"node-02\",\"act\":\"motor\",\"value\":70,\"mode\":\"manual\"}",
              &frame, &command_id) == BRIDGE_MQTT_ERROR_PAYLOAD);
    CHECK(command_id == 1001U);

    command_id = 12345U;
    CHECK(BridgeMqtt_ParseCommandJson(
              "{\"dev\":\"node-02\",\"act\":\"led\",\"value\":70,\"mode\":\"manual\"}",
              &frame, &command_id) == BRIDGE_MQTT_ERROR_PAYLOAD);
    CHECK(command_id == 0U); /* 无 id 可提取 */
    return 0;
}

static int test_formats_error_ack_json(void)
{
    char output[BRIDGE_MQTT_ACK_JSON_BUFFER_SIZE];
    size_t output_length = 0U;

    CHECK(BridgeMqtt_FormatErrorAckJson(1001U, output, sizeof(output),
                                        &output_length) == BRIDGE_MQTT_OK);
    CHECK(strcmp(output,
                 "{\"id\":1001,\"dev\":\"node-02\",\"result\":\"error\"}") == 0);
    CHECK(BridgeMqtt_FormatErrorAckJson(0U, output, sizeof(output),
                                        &output_length) ==
          BRIDGE_MQTT_ERROR_ARGUMENT);
    return 0;
}

static int test_formats_alarm_json(void)
{
    const ProtocolLoraAlarm alarm = {
        .alarm_code = 4U,
        .alarm_level = 2U,
        .active = 1U,
    };
    ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_ALARM,
        .source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID,
        .destination_id = PROTOCOL_LORA_GATEWAY_ID,
        .sequence = 3U,
    };
    char output[BRIDGE_MQTT_ALARM_JSON_BUFFER_SIZE];
    size_t output_length = 0U;

    CHECK(ProtocolLora_SetAlarmPayload(&frame, &alarm) == PROTOCOL_LORA_OK);
    CHECK(BridgeMqtt_FormatAlarmJson(&frame, output, sizeof(output),
                                     &output_length) == BRIDGE_MQTT_OK);
    CHECK(strcmp(output,
                 "{\"dev\":\"node-02\",\"code\":\"CO2_HIGH\",\"active\":true,\"level\":2}") == 0);

    frame.type = PROTOCOL_LORA_FRAME_TELEMETRY;
    CHECK(BridgeMqtt_FormatAlarmJson(&frame, output, sizeof(output),
                                     &output_length) == BRIDGE_MQTT_ERROR_FRAME);
    return 0;
}

static int test_formats_alarm_json_recovery(void)
{
    const ProtocolLoraAlarm alarm = {
        .alarm_code = 3U,
        .alarm_level = 1U,
        .active = 0U,
    };
    ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_ALARM,
        .source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID,
        .destination_id = PROTOCOL_LORA_GATEWAY_ID,
        .sequence = 4U,
    };
    char output[BRIDGE_MQTT_ALARM_JSON_BUFFER_SIZE];
    size_t output_length = 0U;

    CHECK(ProtocolLora_SetAlarmPayload(&frame, &alarm) == PROTOCOL_LORA_OK);
    CHECK(BridgeMqtt_FormatAlarmJson(&frame, output, sizeof(output),
                                     &output_length) == BRIDGE_MQTT_OK);
    CHECK(strcmp(output,
                 "{\"dev\":\"node-02\",\"code\":\"SOIL_DRY\",\"active\":false,\"level\":1}") == 0);
    return 0;
}

static int test_rejects_garbage_alarm_codes(void)
{
    const ProtocolLoraAlarm zero_code = {.alarm_code = 0U, .alarm_level = 1U,
                                         .active = 1U};
    const ProtocolLoraAlarm big_code = {.alarm_code = 8U, .alarm_level = 1U,
                                        .active = 1U};
    const ProtocolLoraAlarm zero_level = {.alarm_code = 4U, .alarm_level = 0U,
                                          .active = 1U};
    ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_ALARM,
        .source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID,
        .destination_id = PROTOCOL_LORA_GATEWAY_ID,
        .sequence = 5U,
    };
    char output[BRIDGE_MQTT_ALARM_JSON_BUFFER_SIZE];
    size_t output_length = 0U;

    CHECK(ProtocolLora_SetAlarmPayload(&frame, &zero_code) == PROTOCOL_LORA_OK);
    CHECK(BridgeMqtt_FormatAlarmJson(&frame, output, sizeof(output),
                                     &output_length) ==
          BRIDGE_MQTT_ERROR_PAYLOAD);
    CHECK(ProtocolLora_SetAlarmPayload(&frame, &big_code) == PROTOCOL_LORA_OK);
    CHECK(BridgeMqtt_FormatAlarmJson(&frame, output, sizeof(output),
                                     &output_length) ==
          BRIDGE_MQTT_ERROR_PAYLOAD);
    CHECK(ProtocolLora_SetAlarmPayload(&frame, &zero_level) == PROTOCOL_LORA_OK);
    CHECK(BridgeMqtt_FormatAlarmJson(&frame, output, sizeof(output),
                                     &output_length) ==
          BRIDGE_MQTT_ERROR_PAYLOAD);
    return 0;
}

static int test_formats_ack_json(void)
{
    ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_ACK,
        .source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID,
        .destination_id = PROTOCOL_LORA_GATEWAY_ID,
        .sequence = 1U,
    };
    const ProtocolLoraAck ack = {
        .command_id = 1001U,
        .result = PROTOCOL_LORA_ACK_OK,
        .actual_value = 70U,
        .device_status = 0U,
    };
    char output[BRIDGE_MQTT_ACK_JSON_BUFFER_SIZE];
    size_t output_length = 0U;

    CHECK(ProtocolLora_SetAckPayload(&frame, &ack) == PROTOCOL_LORA_OK);
    CHECK(BridgeMqtt_FormatAckJson(&frame, output, sizeof(output),
                                   &output_length) == BRIDGE_MQTT_OK);
    CHECK(strcmp(output,
                 "{\"id\":1001,\"dev\":\"node-02\",\"result\":\"ok\",\"actual\":70}") == 0);
    CHECK(output_length == strlen(output));
    return 0;
}

static int test_formats_ack_json_for_off_state(void)
{
    ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_ACK,
        .source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID,
        .destination_id = PROTOCOL_LORA_GATEWAY_ID,
        .sequence = 2U,
    };
    const ProtocolLoraAck ack = {
        .command_id = 1002U,
        .result = PROTOCOL_LORA_ACK_OK,
        .actual_value = 0U,
        .device_status = 0U,
    };
    char output[BRIDGE_MQTT_ACK_JSON_BUFFER_SIZE];
    size_t output_length = 0U;

    CHECK(ProtocolLora_SetAckPayload(&frame, &ack) == PROTOCOL_LORA_OK);
    CHECK(BridgeMqtt_FormatAckJson(&frame, output, sizeof(output),
                                   &output_length) == BRIDGE_MQTT_OK);
    CHECK(strcmp(output,
                 "{\"id\":1002,\"dev\":\"node-02\",\"result\":\"ok\",\"actual\":0}") == 0);
    CHECK(output_length == strlen(output));
    return 0;
}

static int test_formats_status_json(void)
{
    const uint16_t ids[] = {2U, 3U};
    const bool online[] = {true, false};
    char output[BRIDGE_MQTT_STATUS_JSON_BUFFER_SIZE];
    size_t output_length = 0U;

    CHECK(BridgeMqtt_FormatStatusJson(1U, 98U, ids, online, 2U, output,
                                      sizeof(output),
                                      &output_length) == BRIDGE_MQTT_OK);
    CHECK(strcmp(output,
                 "{\"gw\":\"node-01\",\"mqtt\":true,\"link_rate\":98,"
                 "\"terminals\":[{\"dev\":\"node-02\",\"online\":true},"
                 "{\"dev\":\"node-03\",\"online\":false}]}") == 0);

    CHECK(BridgeMqtt_FormatStatusJson(0U, 0U, NULL, NULL, 0U, output,
                                      sizeof(output),
                                      &output_length) == BRIDGE_MQTT_OK);
    CHECK(strcmp(output,
                 "{\"gw\":\"node-01\",\"mqtt\":false,\"link_rate\":0,"
                 "\"terminals\":[]}") == 0);
    return 0;
}

int main(void)
{
    CHECK(test_formats_taskbook_example() == 0);
    CHECK(test_preserves_negative_fraction() == 0);
    CHECK(test_formats_bemfa_sensor_payload() == 0);
    CHECK(test_bemfa_sensor_preserves_negative_fraction() == 0);
    CHECK(test_bemfa_sensor_requires_space_for_terminator() == 0);
    CHECK(test_rejects_wrong_frame_identity() == 0);
    CHECK(test_rejects_bad_payload_length() == 0);
    CHECK(test_requires_space_for_terminator() == 0);
    CHECK(test_maximum_field_values_fit_configured_buffer() == 0);
    CHECK(test_parses_led_command() == 0);
    CHECK(test_parses_led_off_command() == 0);
    CHECK(test_parses_all_five_actuators() == 0);
    CHECK(test_rejects_invalid_command_and_still_gets_id() == 0);
    CHECK(test_formats_error_ack_json() == 0);
    CHECK(test_formats_alarm_json() == 0);
    CHECK(test_formats_alarm_json_recovery() == 0);
    CHECK(test_rejects_garbage_alarm_codes() == 0);
    CHECK(test_formats_ack_json() == 0);
    CHECK(test_formats_ack_json_for_off_state() == 0);
    CHECK(test_formats_status_json() == 0);
    puts("PASS bridge_mqtt");
    return 0;
}
