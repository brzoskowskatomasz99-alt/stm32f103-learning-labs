#ifndef PROTOCOL_LORA_H
#define PROTOCOL_LORA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTOCOL_LORA_SOF                0xA5U
#define PROTOCOL_LORA_VERSION_1          0x01U
#define PROTOCOL_LORA_MAX_PAYLOAD_SIZE   240U
#define PROTOCOL_LORA_HEADER_SIZE        8U
#define PROTOCOL_LORA_CRC_SIZE           2U
#define PROTOCOL_LORA_MAX_FRAME_SIZE     250U
#define PROTOCOL_LORA_GATEWAY_ID         0x0001U
#define PROTOCOL_LORA_FIRST_TERMINAL_ID  0x0002U

typedef enum
{
    PROTOCOL_LORA_FRAME_TELEMETRY = 0x00,
    PROTOCOL_LORA_FRAME_COMMAND = 0x01,
    PROTOCOL_LORA_FRAME_ACK = 0x02,
    PROTOCOL_LORA_FRAME_ALARM = 0x03,
    PROTOCOL_LORA_FRAME_HEARTBEAT = 0x04
} ProtocolLoraFrameType;

typedef enum
{
    PROTOCOL_LORA_OK = 0,
    PROTOCOL_LORA_ERROR_ARGUMENT,
    PROTOCOL_LORA_ERROR_BUFFER_TOO_SMALL,
    PROTOCOL_LORA_ERROR_VERSION,
    PROTOCOL_LORA_ERROR_TYPE,
    PROTOCOL_LORA_ERROR_LENGTH,
    PROTOCOL_LORA_ERROR_SOF,
    PROTOCOL_LORA_ERROR_CRC
} ProtocolLoraStatus;

typedef struct
{
    uint8_t version;
    ProtocolLoraFrameType type;
    uint16_t source_id;
    uint16_t destination_id;
    uint8_t sequence;
    uint8_t payload_length;
    uint8_t payload[PROTOCOL_LORA_MAX_PAYLOAD_SIZE];
} ProtocolLoraFrame;

typedef struct
{
    int16_t temperature_x10;
    uint16_t humidity_x10;
    uint16_t co2_ppm;
    uint16_t lux;
    uint16_t soil_x10;
    uint16_t device_status;
} ProtocolLoraTelemetry;

typedef struct
{
    uint16_t command_id;
    uint8_t actuator;
    uint8_t action;
    uint16_t value;
    uint8_t mode;
} ProtocolLoraCommand;

typedef struct
{
    uint16_t command_id;
    uint8_t result;
    uint16_t actual_value;
    uint16_t device_status;
} ProtocolLoraAck;

typedef struct
{
    uint16_t alarm_code;
    uint8_t alarm_level;
    uint8_t active;
} ProtocolLoraAlarm;

#define PROTOCOL_LORA_ACTUATOR_LED       1U
#define PROTOCOL_LORA_ACTUATOR_BUZZER    2U
#define PROTOCOL_LORA_ACTUATOR_RELAY     3U
#define PROTOCOL_LORA_ACTUATOR_LIGHT_PWM 4U
#define PROTOCOL_LORA_ACTUATOR_FAN_PWM   5U
#define PROTOCOL_LORA_ACTION_OFF         0U
#define PROTOCOL_LORA_ACTION_SET         1U
#define PROTOCOL_LORA_MODE_MANUAL        1U
#define PROTOCOL_LORA_ACK_OK             0U
#define PROTOCOL_LORA_ACK_INVALID        1U

uint16_t ProtocolLora_Crc16Modbus(const uint8_t *data, size_t length);

ProtocolLoraStatus ProtocolLora_Encode(const ProtocolLoraFrame *frame,
                                       uint8_t *output,
                                       size_t output_capacity,
                                       size_t *output_length);

ProtocolLoraStatus ProtocolLora_Decode(const uint8_t *input,
                                       size_t input_length,
                                       ProtocolLoraFrame *frame);

ProtocolLoraStatus ProtocolLora_SetTelemetryPayload(
    ProtocolLoraFrame *frame,
    const ProtocolLoraTelemetry *telemetry);

ProtocolLoraStatus ProtocolLora_GetTelemetryPayload(
    const ProtocolLoraFrame *frame,
    ProtocolLoraTelemetry *telemetry);

ProtocolLoraStatus ProtocolLora_SetCommandPayload(
    ProtocolLoraFrame *frame,
    const ProtocolLoraCommand *command);

ProtocolLoraStatus ProtocolLora_GetCommandPayload(
    const ProtocolLoraFrame *frame,
    ProtocolLoraCommand *command);

ProtocolLoraStatus ProtocolLora_SetAckPayload(
    ProtocolLoraFrame *frame,
    const ProtocolLoraAck *ack);

ProtocolLoraStatus ProtocolLora_GetAckPayload(
    const ProtocolLoraFrame *frame,
    ProtocolLoraAck *ack);

ProtocolLoraStatus ProtocolLora_SetAlarmPayload(
    ProtocolLoraFrame *frame,
    const ProtocolLoraAlarm *alarm);

ProtocolLoraStatus ProtocolLora_GetAlarmPayload(
    const ProtocolLoraFrame *frame,
    ProtocolLoraAlarm *alarm);

#ifdef __cplusplus
}
#endif

#endif
