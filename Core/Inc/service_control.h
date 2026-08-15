#ifndef SERVICE_CONTROL_H
#define SERVICE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "service_hal.h"
#include "terminal_sensors.h"

/*
 * service_control：阈值/滞回/延时确认 + 手/自动/安全仲裁（任务书第 6/8 章）。
 * 优先级：P1 安全保护 > P3 远程手动（30 min 有效） > P2 本地自动 > 默认关。
 */

typedef enum
{
    SERVICE_CONTROL_SOURCE_OFF = 0,
    SERVICE_CONTROL_SOURCE_AUTO,
    SERVICE_CONTROL_SOURCE_MANUAL,
    SERVICE_CONTROL_SOURCE_SAFE
} ServiceControlSource;

bool ServiceControl_Init(void);

/* 每 TERMINAL_CONTROL_TICK_MS 调用一次 */
void ServiceControl_Process(const TerminalSensorSnapshot *snapshot);

ServiceControlSource ServiceControl_GetSource(ServiceActuator actuator);
uint8_t ServiceControl_GetValue(ServiceActuator actuator);

/* 远程手动命令（P3）：value 0=关，1..100=开/设定；成功返回 true 并输出实际值 */
bool ServiceControl_ApplyManualCommand(ServiceActuator actuator,
                                       uint8_t value,
                                       uint16_t *actual);

/* 告警服务驱动蜂鸣器（P2 自动源），每个控制节拍由 service_alarm 更新 */
void ServiceControl_SetAlarmBuzzerRequest(bool on);

#endif /* SERVICE_CONTROL_H */
