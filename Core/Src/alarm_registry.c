#include "alarm_registry.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    uint16_t code;
    uint8_t level;
} AlarmRegistryEntry;

static AlarmRegistryEntry alarm_registry_entries[ALARM_REGISTRY_MAX_ALARMS];
static uint8_t alarm_registry_count = 0U;

bool AlarmRegistry_Init(void)
{
    memset(alarm_registry_entries, 0, sizeof(alarm_registry_entries));
    alarm_registry_count = 0U;
    printf("[ALARMREG] INIT OK\r\n");
    return true;
}

void AlarmRegistry_OnAlarmFrame(const ProtocolLoraFrame *frame)
{
    ProtocolLoraAlarm alarm;
    uint8_t index;

    if ((frame == NULL) || (frame->type != PROTOCOL_LORA_FRAME_ALARM) ||
        (ProtocolLora_GetAlarmPayload(frame, &alarm) != PROTOCOL_LORA_OK))
    {
        return;
    }
    /* 告警码只允许 1..7 且等级非 0：拒绝垃圾帧 */
    if ((alarm.alarm_code < 1U) || (alarm.alarm_code > 7U) ||
        (alarm.alarm_level == 0U))
    {
        printf("[ALARMREG] INVALID CODE=%u LEVEL=%u DROP\r\n",
               (unsigned int)alarm.alarm_code,
               (unsigned int)alarm.alarm_level);
        return;
    }

    if (alarm.active != 0U)
    {
        for (index = 0U; index < alarm_registry_count; ++index)
        {
            if (alarm_registry_entries[index].code == alarm.alarm_code)
            {
                alarm_registry_entries[index].level = alarm.alarm_level;
                return;
            }
        }
        if (alarm_registry_count >= ALARM_REGISTRY_MAX_ALARMS)
        {
            printf("[ALARMREG] FULL DROP CODE=%u\r\n",
                   (unsigned int)alarm.alarm_code);
            return;
        }
        alarm_registry_entries[alarm_registry_count].code = alarm.alarm_code;
        alarm_registry_entries[alarm_registry_count].level = alarm.alarm_level;
        ++alarm_registry_count;
        printf("[ALARMREG] ADD CODE=%u LEVEL=%u\r\n",
               (unsigned int)alarm.alarm_code,
               (unsigned int)alarm.alarm_level);
    }
    else
    {
        for (index = 0U; index < alarm_registry_count; ++index)
        {
            if (alarm_registry_entries[index].code == alarm.alarm_code)
            {
                printf("[ALARMREG] REMOVE CODE=%u\r\n",
                       (unsigned int)alarm.alarm_code);
                alarm_registry_entries[index] =
                    alarm_registry_entries[alarm_registry_count - 1U];
                --alarm_registry_count;
                return;
            }
        }
    }
}

uint8_t AlarmRegistry_GetCount(void)
{
    return alarm_registry_count;
}

bool AlarmRegistry_GetEntry(uint8_t index,
                            uint16_t *alarm_code,
                            uint8_t *alarm_level)
{
    if ((index >= alarm_registry_count) || (alarm_code == NULL) ||
        (alarm_level == NULL))
    {
        return false;
    }
    *alarm_code = alarm_registry_entries[index].code;
    *alarm_level = alarm_registry_entries[index].level;
    return true;
}

uint16_t AlarmRegistry_GetHighestCode(void)
{
    uint8_t index;
    uint8_t highest_index = 0U;

    if (alarm_registry_count == 0U)
    {
        return 0U;
    }
    for (index = 1U; index < alarm_registry_count; ++index)
    {
        if (alarm_registry_entries[index].level >
            alarm_registry_entries[highest_index].level)
        {
            highest_index = index;
        }
    }
    return alarm_registry_entries[highest_index].code;
}
