#ifndef GATEWAY_DATA_H
#define GATEWAY_DATA_H

#include <stdbool.h>
#include <stdint.h>

#include "protocol_lora.h"
#include "ui_oled.h"

/*
 * gateway_data（网关侧）：跨模块数据中枢。
 * mqtt 层写入遥测/ACK/MQTT 状态，UI 层读取聚合数据（任务书第 8 章分层）。
 */

void GatewayData_Init(void);
void GatewayData_NoteTelemetry(const ProtocolLoraFrame *frame,
                               int8_t rssi_dbm,
                               int8_t snr_db);
void GatewayData_NoteAck(uint8_t actuator, const ProtocolLoraAck *ack);
void GatewayData_NoteMqttState(uint8_t connected);
void GatewayData_BuildUiData(UiOledTelemetry *telemetry,
                             UiOledStatus *status);

#endif /* GATEWAY_DATA_H */
