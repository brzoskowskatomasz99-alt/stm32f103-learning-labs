/* command_link 主机单元测试：桩实现 P2P 发送接口与时钟。 */
#include "command_link.h"
#include "service_hal.h"

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

/* ---- 桩 ---- */
static uint32_t g_now_ms = 0U;
static int g_queue_count = 0;
static int g_tx_pending = 0;

bool LLCC68_P2P_QueueFrame(const ProtocolLoraFrame *frame)
{
    (void)frame;
    if (g_tx_pending)
    {
        return false;
    }
    g_tx_pending = 1;
    ++g_queue_count;
    return true;
}

bool LLCC68_P2P_IsTxPending(void)
{
    return g_tx_pending != 0;
}

void ServiceHal_Init(void)
{
}

uint32_t ServiceHal_GetTickMs(void)
{
    return g_now_ms;
}

void ServiceHal_ActuatorWrite(ServiceActuator actuator, uint8_t value)
{
    (void)actuator;
    (void)value;
}

uint8_t ServiceHal_ActuatorRead(ServiceActuator actuator)
{
    (void)actuator;
    return 0U;
}

/* ---- 工具 ---- */
static void StubReset(void)
{
    g_now_ms = 0U;
    g_queue_count = 0;
    g_tx_pending = 0;
}

static void AdvanceMs(uint32_t ms)
{
    g_now_ms += ms;
}

static ProtocolLoraFrame MakeCommand(uint16_t id, uint8_t actuator)
{
    ProtocolLoraFrame frame;
    ProtocolLoraCommand command;

    memset(&frame, 0, sizeof(frame));
    frame.version = PROTOCOL_LORA_VERSION_1;
    frame.type = PROTOCOL_LORA_FRAME_COMMAND;
    frame.source_id = PROTOCOL_LORA_GATEWAY_ID;
    frame.destination_id = PROTOCOL_LORA_FIRST_TERMINAL_ID;
    frame.sequence = (uint8_t)id;
    command.command_id = id;
    command.actuator = actuator;
    command.action = PROTOCOL_LORA_ACTION_SET;
    command.value = 100U;
    command.mode = PROTOCOL_LORA_MODE_MANUAL;
    (void)ProtocolLora_SetCommandPayload(&frame, &command);
    return frame;
}

static ProtocolLoraFrame MakeAckFrame(uint16_t id, uint8_t result,
                                     uint16_t actual)
{
    ProtocolLoraFrame frame;
    ProtocolLoraAck ack;

    memset(&frame, 0, sizeof(frame));
    frame.version = PROTOCOL_LORA_VERSION_1;
    frame.type = PROTOCOL_LORA_FRAME_ACK;
    frame.source_id = PROTOCOL_LORA_FIRST_TERMINAL_ID;
    frame.destination_id = PROTOCOL_LORA_GATEWAY_ID;
    frame.sequence = (uint8_t)id;
    ack.command_id = id;
    ack.result = result;
    ack.actual_value = actual;
    ack.device_status = 0U;
    (void)ProtocolLora_SetAckPayload(&frame, &ack);
    return frame;
}

static int test_send_ack_round_trip(void)
{
    ProtocolLoraFrame command = MakeCommand(77U, PROTOCOL_LORA_ACTUATOR_LED);
    ProtocolLoraFrame mismatch = MakeAckFrame(88U, PROTOCOL_LORA_ACK_OK, 100U);
    ProtocolLoraFrame ack = MakeAckFrame(77U, PROTOCOL_LORA_ACK_OK, 100U);
    CommandLinkResult result;

    CHECK(CommandLink_Init() == true);
    StubReset();
    CHECK(CommandLink_Submit(&command) == true);
    CHECK(CommandLink_IsBusy() == true);
    CommandLink_Process(); /* 发送 */
    CHECK(g_queue_count == 1);
    CHECK(CommandLink_IsBusy() == true);

    /* 不匹配的 ACK 忽略 */
    CommandLink_OnAckFrame(&mismatch);
    CHECK(CommandLink_TakeResult(&result) == false);

    /* 非法 result 的 ACK 忽略（异协议/外机防护） */
    {
        ProtocolLoraFrame bad = MakeAckFrame(77U, 2U, 0U);
        CommandLink_OnAckFrame(&bad);
        CHECK(CommandLink_TakeResult(&result) == false);
        CHECK(CommandLink_IsBusy() == true);
    }

    /* 匹配的 ACK 出结果 */
    CommandLink_OnAckFrame(&ack);
    CHECK(CommandLink_TakeResult(&result) == true);
    CHECK(result.type == COMMAND_LINK_RESULT_ACK);
    CHECK(result.command_id == 77U);
    CHECK(result.actuator == PROTOCOL_LORA_ACTUATOR_LED);
    CHECK(result.ack.result == PROTOCOL_LORA_ACK_OK);
    CHECK(result.ack.actual_value == 100U);
    CHECK(CommandLink_IsBusy() == false);
    return 0;
}

static int test_retry_three_times_then_timeout(void)
{
    ProtocolLoraFrame command = MakeCommand(100U, PROTOCOL_LORA_ACTUATOR_FAN_PWM);
    CommandLinkResult result;

    CHECK(CommandLink_Init() == true);
    StubReset();
    CHECK(CommandLink_Submit(&command) == true);
    CommandLink_Process(); /* 第 1 次发送 */
    CHECK(g_queue_count == 1);

    AdvanceMs(999U);
    CommandLink_Process(); /* 未到 1 s：不重发 */
    CHECK(g_queue_count == 1);

    AdvanceMs(2U); /* t=1001 */
    g_tx_pending = 0; /* 模拟上一帧已发完 */
    CommandLink_Process(); /* 超时 → 第 1 次重发 */
    CHECK(g_queue_count == 2);

    AdvanceMs(1000U);
    g_tx_pending = 0;
    CommandLink_Process(); /* 第 2 次重发 */
    CHECK(g_queue_count == 3);

    AdvanceMs(1000U);
    g_tx_pending = 0;
    CommandLink_Process(); /* 第 3 次重发（最后一次） */
    CHECK(g_queue_count == 4);

    AdvanceMs(1000U);
    g_tx_pending = 0;
    CommandLink_Process(); /* 重发用尽 → 放弃 */
    CHECK(g_queue_count == 4);
    CHECK(CommandLink_TakeResult(&result) == true);
    CHECK(result.type == COMMAND_LINK_RESULT_TIMEOUT);
    CHECK(result.command_id == 100U);
    CHECK(result.actuator == PROTOCOL_LORA_ACTUATOR_FAN_PWM);
    CHECK(CommandLink_IsBusy() == false);
    return 0;
}

static int test_busy_rejects_second_submit(void)
{
    ProtocolLoraFrame first = MakeCommand(1U, PROTOCOL_LORA_ACTUATOR_LED);
    ProtocolLoraFrame second = MakeCommand(2U, PROTOCOL_LORA_ACTUATOR_LED);

    CHECK(CommandLink_Init() == true);
    StubReset();
    CHECK(CommandLink_Submit(&first) == true);
    CHECK(CommandLink_Submit(&second) == false);
    CHECK(CommandLink_Submit(NULL) == false);
    return 0;
}

int main(void)
{
    CHECK(test_send_ack_round_trip() == 0);
    CHECK(test_retry_three_times_then_timeout() == 0);
    CHECK(test_busy_rejects_second_submit() == 0);
    puts("PASS command_link");
    return 0;
}
