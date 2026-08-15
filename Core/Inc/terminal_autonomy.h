#ifndef TERMINAL_AUTONOMY_H
#define TERMINAL_AUTONOMY_H

#include <stdbool.h>

/*
 * terminal_autonomy：终端自治胶水层（任务书第 8 章分层）。
 * 汇集 service_control / service_alarm，并把告警事件组帧经 LoRa 上报网关。
 */

bool TerminalAutonomy_Init(void);
void TerminalAutonomy_Process(void);

#endif /* TERMINAL_AUTONOMY_H */
