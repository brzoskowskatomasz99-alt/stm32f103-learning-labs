#include "terminal_autonomy.h"

#include "llcc68_p2p.h"
#include "protocol_lora.h"
#include "service_alarm.h"
#include "service_control.h"
#include "service_hal.h"
#include "terminal_config.h"
#include "terminal_sensors.h"

#include <stdio.h>
#include <string.h>

static uint32_t autonomy_tick_ms = 0U;
static uint8_t autonomy_sequence = 1U;
static TerminalSensorSnapshot autonomy_snapshot;

bool TerminalAutonomy_Init(void)
{
    ServiceHal_Init();
    if (!ServiceControl_Init())
    {
        printf("[AUTO] CONTROL INIT FAIL\r\n");
        return false;
    }
    if (!ServiceAlarm_Init())
    {
        printf("[AUTO] ALARM INIT FAIL\r\n");
        return false;
    }
    autonomy_tick_ms = ServiceHal_GetTickMs();
    printf("[AUTO] INIT OK\r\n");
    return true;
}

void TerminalAutonomy_Process(void)
{
    uint32_t now = ServiceHal_GetTickMs();
    ServiceAlarmEvent event;

    if (((int32_t)(now - autonomy_tick_ms)) <
        (int32_t)TERMINAL_CONTROL_TICK_MS)
    {
        return;
    }
    autonomy_tick_ms = now;

    if (!TerminalSensors_GetSnapshot(&autonomy_snapshot))
    {
        return;
    }

    ServiceControl_Process(&autonomy_snapshot);
    ServiceAlarm_Process(&autonomy_snapshot);

    while (ServiceAlarm_PeekPendingEvent(&event))
    {
        ProtocolLoraFrame frame;
        ProtocolLoraAlarm alarm;

        memset(&frame, 0, sizeof(frame));
        frame.version = PROTOCOL_LORA_VERSION_1;
        frame.type = PROTOCOL_LORA_FRAME_ALARM;
        frame.source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID;
        frame.destination_id = PROTOCOL_LORA_GATEWAY_ID;
        frame.sequence = autonomy_sequence++;
        alarm.alarm_code = event.code;
        alarm.alarm_level = event.level;
        alarm.active = event.active;
        if (ProtocolLora_SetAlarmPayload(&frame, &alarm) != PROTOCOL_LORA_OK)
        {
            ServiceAlarm_CommitPendingEvent();
            continue;
        }
        if (!LLCC68_P2P_QueueFrame(&frame))
        {
            /* 发送队列忙：事件保留，下一节拍重试 */
            break;
        }
        ServiceAlarm_CommitPendingEvent();
        printf("[ALARM] SEND CODE=%u LEVEL=%u ACTIVE=%u SEQ=%u\r\n",
               (unsigned int)event.code, (unsigned int)event.level,
               (unsigned int)event.active, (unsigned int)frame.sequence);
    }
}
