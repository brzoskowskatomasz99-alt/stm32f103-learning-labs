#ifndef BRIDGE_MQTT_H
#define BRIDGE_MQTT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol_lora.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BRIDGE_MQTT_JSON_BUFFER_SIZE 128U
#define BRIDGE_MQTT_BEMFA_SENSOR_BUFFER_SIZE 40U
#define BRIDGE_MQTT_ACK_JSON_BUFFER_SIZE 96U
#define BRIDGE_MQTT_ALARM_JSON_BUFFER_SIZE 96U
#define BRIDGE_MQTT_STATUS_JSON_BUFFER_SIZE 192U

typedef enum
{
    BRIDGE_MQTT_OK = 0,
    BRIDGE_MQTT_ERROR_ARGUMENT,
    BRIDGE_MQTT_ERROR_FRAME,
    BRIDGE_MQTT_ERROR_SOURCE,
    BRIDGE_MQTT_ERROR_DESTINATION,
    BRIDGE_MQTT_ERROR_PAYLOAD,
    BRIDGE_MQTT_ERROR_BUFFER_TOO_SMALL
} BridgeMqttStatus;

BridgeMqttStatus BridgeMqtt_FormatTelemetryJson(
    const ProtocolLoraFrame *frame,
    int8_t rssi_dbm,
    char *output,
    size_t output_capacity,
    size_t *output_length);

BridgeMqttStatus BridgeMqtt_FormatBemfaSensor(
    const ProtocolLoraFrame *frame,
    char *output,
    size_t output_capacity,
    size_t *output_length);

/* 通用命令解析：支持 led/buzzer/relay/light/fan 五类执行器。
   成功时填充 command frame 并输出 command_id；失败时 command_id 仍尽力提取
   （用于发布错误 ACK）。 */
BridgeMqttStatus BridgeMqtt_ParseCommandJson(
    const char *json,
    ProtocolLoraFrame *frame,
    uint16_t *command_id);

BridgeMqttStatus BridgeMqtt_FormatAckJson(
    const ProtocolLoraFrame *frame,
    char *output,
    size_t output_capacity,
    size_t *output_length);

BridgeMqttStatus BridgeMqtt_FormatAckJsonFromAck(
    const ProtocolLoraAck *ack,
    char *output,
    size_t output_capacity,
    size_t *output_length);

BridgeMqttStatus BridgeMqtt_FormatErrorAckJson(
    uint16_t command_id,
    char *output,
    size_t output_capacity,
    size_t *output_length);

/* 告警码 → 任务书字符串（LIGHT_LOW/SOIL_WET/SOIL_DRY/CO2_HIGH/TEMP_ALARM/
   HUMI_ALARM/SENSOR_FAULT，未知返回 UNKNOWN） */
const char *BridgeMqtt_AlarmCodeString(uint16_t code);

BridgeMqttStatus BridgeMqtt_FormatAlarmJson(
    const ProtocolLoraFrame *frame,
    char *output,
    size_t output_capacity,
    size_t *output_length);

/* 网关状态 JSON（含终端在线表）：mqtt_connected 0/1，link_rate 0..100 */
BridgeMqttStatus BridgeMqtt_FormatStatusJson(
    uint8_t mqtt_connected,
    uint8_t link_rate_percent,
    const uint16_t *terminal_ids,
    const bool *terminal_online,
    uint8_t terminal_count,
    char *output,
    size_t output_capacity,
    size_t *output_length);

#ifdef __cplusplus
}
#endif

#endif
