#ifndef TERMINAL_TABLE_H
#define TERMINAL_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "gateway_config.h"

/*
 * terminal_table（网关侧）：终端在线表（任务书第 8 章）。
 * 收到有效遥测即更新最后上报时间；超过 GATEWAY_TERMINAL_OFFLINE_MS 判离线。
 */

#define TERMINAL_TABLE_MAX_TERMINALS 8U

bool TerminalTable_Init(void);
void TerminalTable_NoteTelemetry(uint16_t terminal_id, uint32_t now_ms);
bool TerminalTable_IsOnline(uint16_t terminal_id, uint32_t now_ms);
uint8_t TerminalTable_GetCount(void);
bool TerminalTable_GetEntry(uint8_t index,
                            uint16_t *terminal_id,
                            bool *online,
                            uint32_t now_ms);

#endif /* TERMINAL_TABLE_H */
