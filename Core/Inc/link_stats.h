#ifndef LINK_STATS_H
#define LINK_STATS_H

#include <stdbool.h>
#include <stdint.h>

#include "gateway_config.h"

/*
 * link_stats（网关侧）：LoRa 遥测链路成功率（任务书 T01：
 * 连续 50 次上报成功率 >= 95%）。
 *
 * 口径（任务书 T01）：终端每 20 s 应上报一次遥测。每 20 s 推进一个时隙
 * （默认"未收到"），收到有效遥测时把该时隙标记为"收到"。
 * 成功率 = 窗口内收到的时隙数 / 已推进的时隙数。噪声帧不计入分母。
 */

bool LinkStats_Init(void);

/* 每个主循环节拍调用：按 20 s 节拍推进时隙 */
void LinkStats_Process(uint32_t now_ms);

/* 收到有效遥测帧时调用（标记当前周期成功） */
void LinkStats_NoteTelemetry(void);

uint16_t LinkStats_GetTotalTelemetry(void);
uint16_t LinkStats_GetWindowSlots(void);
uint8_t LinkStats_GetSuccessRatePercent(void);

#endif /* LINK_STATS_H */
