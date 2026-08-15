#ifndef ALARM_REGISTRY_H
#define ALARM_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

#include "protocol_lora.h"

/*
 * alarm_registry（网关侧）：当前活动告警登记表。
 * 由收到的 ALARM 帧维护（active=1 登记，active=0 移除），供 OLED P4 报警页与
 * 告警计数使用。
 */

#define ALARM_REGISTRY_MAX_ALARMS 7U

bool AlarmRegistry_Init(void);
void AlarmRegistry_OnAlarmFrame(const ProtocolLoraFrame *frame);
uint8_t AlarmRegistry_GetCount(void);
bool AlarmRegistry_GetEntry(uint8_t index,
                            uint16_t *alarm_code,
                            uint8_t *alarm_level);
uint16_t AlarmRegistry_GetHighestCode(void);

#endif /* ALARM_REGISTRY_H */
