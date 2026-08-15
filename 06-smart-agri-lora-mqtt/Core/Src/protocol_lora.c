#include "protocol_lora.h"

#include <string.h>

static int ProtocolLora_IsFrameTypeValid(ProtocolLoraFrameType type)
{
    return type <= PROTOCOL_LORA_FRAME_HEARTBEAT;
}

static void ProtocolLora_WriteU16(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)(value >> 8U);
    output[1] = (uint8_t)value;
}

static uint16_t ProtocolLora_ReadU16(const uint8_t *input)
{
    return (uint16_t)(((uint16_t)input[0] << 8U) | input[1]);
}

uint16_t ProtocolLora_Crc16Modbus(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFU;
    size_t index;
    uint8_t bit;

    if ((data == NULL) && (length != 0U))
    {
        return 0U;
    }

    for (index = 0U; index < length; ++index)
    {
        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
            }
            else
            {
                crc >>= 1U;
            }
        }
    }
    return crc;
}

ProtocolLoraStatus ProtocolLora_Encode(const ProtocolLoraFrame *frame,
                                       uint8_t *output,
                                       size_t output_capacity,
                                       size_t *output_length)
{
    size_t frame_length;
    uint16_t crc;

    if ((frame == NULL) || (output == NULL) || (output_length == NULL))
    {
        return PROTOCOL_LORA_ERROR_ARGUMENT;
    }
    *output_length = 0U;
    if (frame->version != PROTOCOL_LORA_VERSION_1)
    {
        return PROTOCOL_LORA_ERROR_VERSION;
    }
    if (!ProtocolLora_IsFrameTypeValid(frame->type))
    {
        return PROTOCOL_LORA_ERROR_TYPE;
    }
    if (frame->payload_length > PROTOCOL_LORA_MAX_PAYLOAD_SIZE)
    {
        return PROTOCOL_LORA_ERROR_LENGTH;
    }

    frame_length = PROTOCOL_LORA_HEADER_SIZE + frame->payload_length +
                   PROTOCOL_LORA_CRC_SIZE;
    if (output_capacity < frame_length)
    {
        return PROTOCOL_LORA_ERROR_BUFFER_TOO_SMALL;
    }

    output[0] = PROTOCOL_LORA_SOF;
    output[1] = (uint8_t)((frame->version << 4U) | (uint8_t)frame->type);
    output[2] = (uint8_t)(frame->source_id >> 8U);
    output[3] = (uint8_t)frame->source_id;
    output[4] = (uint8_t)(frame->destination_id >> 8U);
    output[5] = (uint8_t)frame->destination_id;
    output[6] = frame->sequence;
    output[7] = frame->payload_length;
    if (frame->payload_length != 0U)
    {
        memcpy(&output[PROTOCOL_LORA_HEADER_SIZE], frame->payload,
               frame->payload_length);
    }

    crc = ProtocolLora_Crc16Modbus(&output[1],
                                   7U + frame->payload_length);
    output[frame_length - 2U] = (uint8_t)(crc >> 8U);
    output[frame_length - 1U] = (uint8_t)crc;
    *output_length = frame_length;
    return PROTOCOL_LORA_OK;
}

ProtocolLoraStatus ProtocolLora_Decode(const uint8_t *input,
                                       size_t input_length,
                                       ProtocolLoraFrame *frame)
{
    uint8_t version;
    ProtocolLoraFrameType type;
    uint8_t payload_length;
    size_t expected_length;
    uint16_t expected_crc;
    uint16_t actual_crc;

    if ((input == NULL) || (frame == NULL))
    {
        return PROTOCOL_LORA_ERROR_ARGUMENT;
    }
    if (input_length < (PROTOCOL_LORA_HEADER_SIZE + PROTOCOL_LORA_CRC_SIZE))
    {
        return PROTOCOL_LORA_ERROR_LENGTH;
    }
    if (input[0] != PROTOCOL_LORA_SOF)
    {
        return PROTOCOL_LORA_ERROR_SOF;
    }

    version = (uint8_t)(input[1] >> 4U);
    type = (ProtocolLoraFrameType)(input[1] & 0x0FU);
    if (version != PROTOCOL_LORA_VERSION_1)
    {
        return PROTOCOL_LORA_ERROR_VERSION;
    }
    if (!ProtocolLora_IsFrameTypeValid(type))
    {
        return PROTOCOL_LORA_ERROR_TYPE;
    }

    payload_length = input[7];
    if (payload_length > PROTOCOL_LORA_MAX_PAYLOAD_SIZE)
    {
        return PROTOCOL_LORA_ERROR_LENGTH;
    }
    expected_length = PROTOCOL_LORA_HEADER_SIZE + payload_length +
                      PROTOCOL_LORA_CRC_SIZE;
    if (input_length != expected_length)
    {
        return PROTOCOL_LORA_ERROR_LENGTH;
    }

    expected_crc = (uint16_t)(((uint16_t)input[input_length - 2U] << 8U) |
                              input[input_length - 1U]);
    actual_crc = ProtocolLora_Crc16Modbus(&input[1], 7U + payload_length);
    if (actual_crc != expected_crc)
    {
        return PROTOCOL_LORA_ERROR_CRC;
    }

    frame->version = version;
    frame->type = type;
    frame->source_id = (uint16_t)(((uint16_t)input[2] << 8U) | input[3]);
    frame->destination_id = (uint16_t)(((uint16_t)input[4] << 8U) | input[5]);
    frame->sequence = input[6];
    frame->payload_length = payload_length;
    if (payload_length != 0U)
    {
        memcpy(frame->payload, &input[PROTOCOL_LORA_HEADER_SIZE], payload_length);
    }
    return PROTOCOL_LORA_OK;
}

ProtocolLoraStatus ProtocolLora_SetTelemetryPayload(
    ProtocolLoraFrame *frame,
    const ProtocolLoraTelemetry *telemetry)
{
    if ((frame == NULL) || (telemetry == NULL))
    {
        return PROTOCOL_LORA_ERROR_ARGUMENT;
    }
    if (frame->type != PROTOCOL_LORA_FRAME_TELEMETRY)
    {
        return PROTOCOL_LORA_ERROR_TYPE;
    }

    ProtocolLora_WriteU16(&frame->payload[0], (uint16_t)telemetry->temperature_x10);
    ProtocolLora_WriteU16(&frame->payload[2], telemetry->humidity_x10);
    ProtocolLora_WriteU16(&frame->payload[4], telemetry->co2_ppm);
    ProtocolLora_WriteU16(&frame->payload[6], telemetry->lux);
    ProtocolLora_WriteU16(&frame->payload[8], telemetry->soil_x10);
    ProtocolLora_WriteU16(&frame->payload[10], telemetry->device_status);
    frame->payload_length = 12U;
    return PROTOCOL_LORA_OK;
}

ProtocolLoraStatus ProtocolLora_GetTelemetryPayload(
    const ProtocolLoraFrame *frame,
    ProtocolLoraTelemetry *telemetry)
{
    if ((frame == NULL) || (telemetry == NULL))
    {
        return PROTOCOL_LORA_ERROR_ARGUMENT;
    }
    if (frame->type != PROTOCOL_LORA_FRAME_TELEMETRY)
    {
        return PROTOCOL_LORA_ERROR_TYPE;
    }
    if (frame->payload_length != 12U)
    {
        return PROTOCOL_LORA_ERROR_LENGTH;
    }

    telemetry->temperature_x10 = (int16_t)ProtocolLora_ReadU16(&frame->payload[0]);
    telemetry->humidity_x10 = ProtocolLora_ReadU16(&frame->payload[2]);
    telemetry->co2_ppm = ProtocolLora_ReadU16(&frame->payload[4]);
    telemetry->lux = ProtocolLora_ReadU16(&frame->payload[6]);
    telemetry->soil_x10 = ProtocolLora_ReadU16(&frame->payload[8]);
    telemetry->device_status = ProtocolLora_ReadU16(&frame->payload[10]);
    return PROTOCOL_LORA_OK;
}

ProtocolLoraStatus ProtocolLora_SetCommandPayload(
    ProtocolLoraFrame *frame,
    const ProtocolLoraCommand *command)
{
    if ((frame == NULL) || (command == NULL))
    {
        return PROTOCOL_LORA_ERROR_ARGUMENT;
    }
    if (frame->type != PROTOCOL_LORA_FRAME_COMMAND)
    {
        return PROTOCOL_LORA_ERROR_TYPE;
    }

    ProtocolLora_WriteU16(&frame->payload[0], command->command_id);
    frame->payload[2] = command->actuator;
    frame->payload[3] = command->action;
    ProtocolLora_WriteU16(&frame->payload[4], command->value);
    frame->payload[6] = command->mode;
    frame->payload_length = 7U;
    return PROTOCOL_LORA_OK;
}

ProtocolLoraStatus ProtocolLora_GetCommandPayload(
    const ProtocolLoraFrame *frame,
    ProtocolLoraCommand *command)
{
    if ((frame == NULL) || (command == NULL))
    {
        return PROTOCOL_LORA_ERROR_ARGUMENT;
    }
    if (frame->type != PROTOCOL_LORA_FRAME_COMMAND)
    {
        return PROTOCOL_LORA_ERROR_TYPE;
    }
    if (frame->payload_length != 7U)
    {
        return PROTOCOL_LORA_ERROR_LENGTH;
    }

    command->command_id = ProtocolLora_ReadU16(&frame->payload[0]);
    command->actuator = frame->payload[2];
    command->action = frame->payload[3];
    command->value = ProtocolLora_ReadU16(&frame->payload[4]);
    command->mode = frame->payload[6];
    return PROTOCOL_LORA_OK;
}

ProtocolLoraStatus ProtocolLora_SetAckPayload(
    ProtocolLoraFrame *frame,
    const ProtocolLoraAck *ack)
{
    if ((frame == NULL) || (ack == NULL))
    {
        return PROTOCOL_LORA_ERROR_ARGUMENT;
    }
    if (frame->type != PROTOCOL_LORA_FRAME_ACK)
    {
        return PROTOCOL_LORA_ERROR_TYPE;
    }

    ProtocolLora_WriteU16(&frame->payload[0], ack->command_id);
    frame->payload[2] = ack->result;
    ProtocolLora_WriteU16(&frame->payload[3], ack->actual_value);
    ProtocolLora_WriteU16(&frame->payload[5], ack->device_status);
    frame->payload_length = 7U;
    return PROTOCOL_LORA_OK;
}

ProtocolLoraStatus ProtocolLora_GetAckPayload(
    const ProtocolLoraFrame *frame,
    ProtocolLoraAck *ack)
{
    if ((frame == NULL) || (ack == NULL))
    {
        return PROTOCOL_LORA_ERROR_ARGUMENT;
    }
    if (frame->type != PROTOCOL_LORA_FRAME_ACK)
    {
        return PROTOCOL_LORA_ERROR_TYPE;
    }
    if (frame->payload_length != 7U)
    {
        return PROTOCOL_LORA_ERROR_LENGTH;
    }

    ack->command_id = ProtocolLora_ReadU16(&frame->payload[0]);
    ack->result = frame->payload[2];
    ack->actual_value = ProtocolLora_ReadU16(&frame->payload[3]);
    ack->device_status = ProtocolLora_ReadU16(&frame->payload[5]);
    return PROTOCOL_LORA_OK;
}

ProtocolLoraStatus ProtocolLora_SetAlarmPayload(
    ProtocolLoraFrame *frame,
    const ProtocolLoraAlarm *alarm)
{
    if ((frame == NULL) || (alarm == NULL))
    {
        return PROTOCOL_LORA_ERROR_ARGUMENT;
    }
    if (frame->type != PROTOCOL_LORA_FRAME_ALARM)
    {
        return PROTOCOL_LORA_ERROR_TYPE;
    }

    ProtocolLora_WriteU16(&frame->payload[0], alarm->alarm_code);
    frame->payload[2] = alarm->alarm_level;
    frame->payload[3] = alarm->active;
    frame->payload_length = 4U;
    return PROTOCOL_LORA_OK;
}

ProtocolLoraStatus ProtocolLora_GetAlarmPayload(
    const ProtocolLoraFrame *frame,
    ProtocolLoraAlarm *alarm)
{
    if ((frame == NULL) || (alarm == NULL))
    {
        return PROTOCOL_LORA_ERROR_ARGUMENT;
    }
    if (frame->type != PROTOCOL_LORA_FRAME_ALARM)
    {
        return PROTOCOL_LORA_ERROR_TYPE;
    }
    if (frame->payload_length != 4U)
    {
        return PROTOCOL_LORA_ERROR_LENGTH;
    }

    alarm->alarm_code = ProtocolLora_ReadU16(&frame->payload[0]);
    alarm->alarm_level = frame->payload[2];
    alarm->active = frame->payload[3];
    return PROTOCOL_LORA_OK;
}
