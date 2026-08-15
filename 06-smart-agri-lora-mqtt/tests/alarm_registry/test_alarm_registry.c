/* alarm_registry 主机单元测试。 */
#include "alarm_registry.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
    do                                                                          \
    {                                                                           \
        if (!(condition))                                                       \
        {                                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);        \
            return 1;                                                           \
        }                                                                       \
    } while (0)

static ProtocolLoraFrame MakeAlarmFrame(uint16_t code, uint8_t level,
                                        uint8_t active)
{
    ProtocolLoraFrame frame;
    ProtocolLoraAlarm alarm;

    memset(&frame, 0, sizeof(frame));
    frame.version = PROTOCOL_LORA_VERSION_1;
    frame.type = PROTOCOL_LORA_FRAME_ALARM;
    frame.source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID;
    frame.destination_id = PROTOCOL_LORA_GATEWAY_ID;
    frame.sequence = 1U;
    alarm.alarm_code = code;
    alarm.alarm_level = level;
    alarm.active = active;
    (void)ProtocolLora_SetAlarmPayload(&frame, &alarm);
    return frame;
}

static int test_add_update_remove(void)
{
    ProtocolLoraFrame frame;
    uint16_t code = 0U;
    uint8_t level = 0U;

    CHECK(AlarmRegistry_Init() == true);
    CHECK(AlarmRegistry_GetCount() == 0U);
    CHECK(AlarmRegistry_GetHighestCode() == 0U);

    frame = MakeAlarmFrame(4U, 1U, 1U);
    AlarmRegistry_OnAlarmFrame(&frame);
    CHECK(AlarmRegistry_GetCount() == 1U);
    CHECK(AlarmRegistry_GetEntry(0U, &code, &level) == true);
    CHECK(code == 4U);
    CHECK(level == 1U);

    frame = MakeAlarmFrame(4U, 2U, 1U); /* 等级升级 */
    AlarmRegistry_OnAlarmFrame(&frame);
    CHECK(AlarmRegistry_GetCount() == 1U);
    CHECK(AlarmRegistry_GetEntry(0U, &code, &level) == true);
    CHECK(level == 2U);

    frame = MakeAlarmFrame(6U, 1U, 1U);
    AlarmRegistry_OnAlarmFrame(&frame);
    CHECK(AlarmRegistry_GetCount() == 2U);
    CHECK(AlarmRegistry_GetHighestCode() == 4U); /* 等级 2 优先 */

    frame = MakeAlarmFrame(4U, 2U, 0U); /* 恢复 */
    AlarmRegistry_OnAlarmFrame(&frame);
    CHECK(AlarmRegistry_GetCount() == 1U);
    CHECK(AlarmRegistry_GetEntry(0U, &code, &level) == true);
    CHECK(code == 6U);
    CHECK(AlarmRegistry_GetHighestCode() == 6U);

    frame = MakeAlarmFrame(6U, 1U, 0U);
    AlarmRegistry_OnAlarmFrame(&frame);
    CHECK(AlarmRegistry_GetCount() == 0U);
    return 0;
}

static int test_capacity(void)
{
    ProtocolLoraFrame frame;
    unsigned int i;

    CHECK(AlarmRegistry_Init() == true);
    for (i = 0U; i < ALARM_REGISTRY_MAX_ALARMS; ++i)
    {
        frame = MakeAlarmFrame((uint16_t)(1U + i), 1U, 1U);
        AlarmRegistry_OnAlarmFrame(&frame);
    }
    CHECK(AlarmRegistry_GetCount() == ALARM_REGISTRY_MAX_ALARMS);
    frame = MakeAlarmFrame(99U, 2U, 1U); /* 满：丢弃 */
    AlarmRegistry_OnAlarmFrame(&frame);
    CHECK(AlarmRegistry_GetCount() == ALARM_REGISTRY_MAX_ALARMS);
    return 0;
}

static int test_rejects_garbage_frames(void)
{
    ProtocolLoraFrame frame;

    CHECK(AlarmRegistry_Init() == true);
    frame = MakeAlarmFrame(0U, 1U, 1U); /* 码 0 */
    AlarmRegistry_OnAlarmFrame(&frame);
    frame = MakeAlarmFrame(8U, 1U, 1U); /* 码超界 */
    AlarmRegistry_OnAlarmFrame(&frame);
    frame = MakeAlarmFrame(4U, 0U, 1U); /* 等级 0 */
    AlarmRegistry_OnAlarmFrame(&frame);
    CHECK(AlarmRegistry_GetCount() == 0U);
    return 0;
}

int main(void)
{
    CHECK(test_add_update_remove() == 0);
    CHECK(test_capacity() == 0);
    CHECK(test_rejects_garbage_frames() == 0);
    puts("PASS alarm_registry");
    return 0;
}
