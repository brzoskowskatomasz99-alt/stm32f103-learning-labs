#include "protocol_lora.h"

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

static int test_encode_empty_heartbeat_frame(void)
{
    const ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_HEARTBEAT,
        .source_id = 0x0002U,
        .destination_id = 0x0001U,
        .sequence = 0x18U,
        .payload_length = 0U,
    };
    const uint8_t expected[] = {
        0xA5U, 0x14U, 0x00U, 0x02U, 0x00U, 0x01U, 0x18U, 0x00U, 0x01U, 0x6DU,
    };
    uint8_t encoded[PROTOCOL_LORA_MAX_FRAME_SIZE];
    size_t encoded_length = 0U;

    CHECK(ProtocolLora_Encode(&frame, encoded, sizeof(encoded), &encoded_length) ==
          PROTOCOL_LORA_OK);
    CHECK(encoded_length == sizeof(expected));
    CHECK(memcmp(encoded, expected, sizeof(expected)) == 0);
    return 0;
}

static int test_decode_frame_and_reject_bad_crc(void)
{
    const uint8_t encoded[] = {
        0xA5U, 0x11U, 0x00U, 0x01U, 0x00U, 0x02U, 0x7EU, 0x05U,
        0x12U, 0x34U, 0x02U, 0x01U, 0x64U, 0xF8U, 0x01U,
    };
    uint8_t damaged[sizeof(encoded)];
    ProtocolLoraFrame frame;

    CHECK(ProtocolLora_Decode(encoded, sizeof(encoded), &frame) == PROTOCOL_LORA_OK);
    CHECK(frame.version == PROTOCOL_LORA_VERSION_1);
    CHECK(frame.type == PROTOCOL_LORA_FRAME_COMMAND);
    CHECK(frame.source_id == 0x0001U);
    CHECK(frame.destination_id == 0x0002U);
    CHECK(frame.sequence == 0x7EU);
    CHECK(frame.payload_length == 5U);
    CHECK(memcmp(frame.payload, &encoded[PROTOCOL_LORA_HEADER_SIZE], 5U) == 0);

    memcpy(damaged, encoded, sizeof(damaged));
    damaged[9] ^= 0x01U;
    CHECK(ProtocolLora_Decode(damaged, sizeof(damaged), &frame) ==
          PROTOCOL_LORA_ERROR_CRC);
    return 0;
}

static int test_telemetry_payload_uses_big_endian_fields(void)
{
    const ProtocolLoraTelemetry telemetry = {
        .temperature_x10 = -55,
        .humidity_x10 = 621U,
        .co2_ppm = 860U,
        .lux = 420U,
        .soil_x10 = 485U,
        .device_status = 0x0123U,
    };
    const uint8_t expected[] = {
        0xFFU, 0xC9U, 0x02U, 0x6DU, 0x03U, 0x5CU,
        0x01U, 0xA4U, 0x01U, 0xE5U, 0x01U, 0x23U,
    };
    ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_TELEMETRY,
    };
    ProtocolLoraTelemetry decoded;

    CHECK(ProtocolLora_SetTelemetryPayload(&frame, &telemetry) == PROTOCOL_LORA_OK);
    CHECK(frame.payload_length == sizeof(expected));
    CHECK(memcmp(frame.payload, expected, sizeof(expected)) == 0);
    CHECK(ProtocolLora_GetTelemetryPayload(&frame, &decoded) == PROTOCOL_LORA_OK);
    CHECK(memcmp(&decoded, &telemetry, sizeof(telemetry)) == 0);
    return 0;
}

static int test_command_payload_round_trip(void)
{
    const ProtocolLoraCommand command = {
        .command_id = 1001U,
        .actuator = 4U,
        .action = 1U,
        .value = 70U,
        .mode = 1U,
    };
    const uint8_t expected[] = {0x03U, 0xE9U, 0x04U, 0x01U, 0x00U, 0x46U, 0x01U};
    ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_COMMAND,
    };
    ProtocolLoraCommand decoded;

    CHECK(ProtocolLora_SetCommandPayload(&frame, &command) == PROTOCOL_LORA_OK);
    CHECK(frame.payload_length == sizeof(expected));
    CHECK(memcmp(frame.payload, expected, sizeof(expected)) == 0);
    CHECK(ProtocolLora_GetCommandPayload(&frame, &decoded) == PROTOCOL_LORA_OK);
    CHECK(decoded.command_id == command.command_id);
    CHECK(decoded.actuator == command.actuator);
    CHECK(decoded.action == command.action);
    CHECK(decoded.value == command.value);
    CHECK(decoded.mode == command.mode);
    return 0;
}

static int test_command_payload_off_round_trip(void)
{
    const ProtocolLoraCommand command = {
        .command_id = 1002U,
        .actuator = PROTOCOL_LORA_ACTUATOR_LED,
        .action = PROTOCOL_LORA_ACTION_OFF,
        .value = 0U,
        .mode = PROTOCOL_LORA_MODE_MANUAL,
    };
    const uint8_t expected[] = {0x03U, 0xEAU, 0x01U, 0x00U, 0x00U, 0x00U, 0x01U};
    ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_COMMAND,
    };
    ProtocolLoraCommand decoded;

    CHECK(ProtocolLora_SetCommandPayload(&frame, &command) == PROTOCOL_LORA_OK);
    CHECK(frame.payload_length == sizeof(expected));
    CHECK(memcmp(frame.payload, expected, sizeof(expected)) == 0);
    CHECK(ProtocolLora_GetCommandPayload(&frame, &decoded) == PROTOCOL_LORA_OK);
    CHECK(decoded.command_id == command.command_id);
    CHECK(decoded.actuator == command.actuator);
    CHECK(decoded.action == command.action);
    CHECK(decoded.value == command.value);
    CHECK(decoded.mode == command.mode);
    return 0;
}

static int test_ack_payload_round_trip(void)
{
    const ProtocolLoraAck ack = {
        .command_id = 1001U,
        .result = 0U,
        .actual_value = 70U,
        .device_status = 0x0005U,
    };
    const uint8_t expected[] = {0x03U, 0xE9U, 0x00U, 0x00U, 0x46U, 0x00U, 0x05U};
    ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_ACK,
    };
    ProtocolLoraAck decoded;

    CHECK(ProtocolLora_SetAckPayload(&frame, &ack) == PROTOCOL_LORA_OK);
    CHECK(frame.payload_length == sizeof(expected));
    CHECK(memcmp(frame.payload, expected, sizeof(expected)) == 0);
    CHECK(ProtocolLora_GetAckPayload(&frame, &decoded) == PROTOCOL_LORA_OK);
    CHECK(decoded.command_id == ack.command_id);
    CHECK(decoded.result == ack.result);
    CHECK(decoded.actual_value == ack.actual_value);
    CHECK(decoded.device_status == ack.device_status);
    return 0;
}

static int test_rejects_oversized_payload_before_copy(void)
{
    uint8_t encoded[PROTOCOL_LORA_MAX_FRAME_SIZE + 1U] = {0};
    ProtocolLoraFrame frame;

    encoded[0] = PROTOCOL_LORA_SOF;
    encoded[1] = 0x10U;
    encoded[7] = 241U;
    CHECK(ProtocolLora_Decode(encoded, sizeof(encoded), &frame) ==
          PROTOCOL_LORA_ERROR_LENGTH);
    return 0;
}

static int test_alarm_payload_round_trip(void)
{
    const ProtocolLoraAlarm alarm = {
        .alarm_code = 4U,
        .alarm_level = 2U,
        .active = 1U,
    };
    const uint8_t expected[] = {0x00U, 0x04U, 0x02U, 0x01U};
    ProtocolLoraFrame frame = {
        .version = PROTOCOL_LORA_VERSION_1,
        .type = PROTOCOL_LORA_FRAME_ALARM,
    };
    ProtocolLoraAlarm decoded;

    CHECK(ProtocolLora_SetAlarmPayload(&frame, &alarm) == PROTOCOL_LORA_OK);
    CHECK(frame.payload_length == sizeof(expected));
    CHECK(memcmp(frame.payload, expected, sizeof(expected)) == 0);
    CHECK(ProtocolLora_GetAlarmPayload(&frame, &decoded) == PROTOCOL_LORA_OK);
    CHECK(decoded.alarm_code == alarm.alarm_code);
    CHECK(decoded.alarm_level == alarm.alarm_level);
    CHECK(decoded.active == alarm.active);

    /* 类型不匹配必须拒绝 */
    frame.type = PROTOCOL_LORA_FRAME_TELEMETRY;
    CHECK(ProtocolLora_SetAlarmPayload(&frame, &alarm) ==
          PROTOCOL_LORA_ERROR_TYPE);
    frame.type = PROTOCOL_LORA_FRAME_ALARM;
    frame.payload_length = 5U;
    CHECK(ProtocolLora_GetAlarmPayload(&frame, &decoded) ==
          PROTOCOL_LORA_ERROR_LENGTH);
    return 0;
}

int main(void)
{
    CHECK(test_encode_empty_heartbeat_frame() == 0);
    CHECK(test_decode_frame_and_reject_bad_crc() == 0);
    CHECK(test_telemetry_payload_uses_big_endian_fields() == 0);
    CHECK(test_command_payload_round_trip() == 0);
    CHECK(test_command_payload_off_round_trip() == 0);
    CHECK(test_ack_payload_round_trip() == 0);
    CHECK(test_alarm_payload_round_trip() == 0);
    CHECK(test_rejects_oversized_payload_before_copy() == 0);
    puts("PASS protocol_lora");
    return 0;
}
