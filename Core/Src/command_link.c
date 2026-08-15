#include "command_link.h"

#include "gateway_config.h"
#include "llcc68_p2p.h"
#include "service_hal.h"

#include <stdio.h>
#include <string.h>

#define COMMAND_LINK_RESULT_QUEUE_SIZE 4U

typedef enum
{
    COMMAND_LINK_IDLE = 0,
    COMMAND_LINK_WAIT_ACK
} CommandLinkState;

static CommandLinkState command_link_state = COMMAND_LINK_IDLE;
static ProtocolLoraFrame command_link_pending;
static bool command_link_pending_valid = false;
static uint16_t command_link_pending_id = 0U;
static uint8_t command_link_pending_actuator = 0U;
static uint8_t command_link_retries = 0U;
static uint32_t command_link_sent_ms = 0U;
static CommandLinkResult command_link_results[COMMAND_LINK_RESULT_QUEUE_SIZE];
static uint8_t command_link_result_head = 0U;
static uint8_t command_link_result_count = 0U;

static void CommandLinkPushResult(const CommandLinkResult *result)
{
    if (command_link_result_count >= COMMAND_LINK_RESULT_QUEUE_SIZE)
    {
        printf("[CMDLINK] RESULT QUEUE FULL\r\n");
        return;
    }
    command_link_results[(command_link_result_head +
                          command_link_result_count) %
                         COMMAND_LINK_RESULT_QUEUE_SIZE] = *result;
    ++command_link_result_count;
}

bool CommandLink_Init(void)
{
    command_link_state = COMMAND_LINK_IDLE;
    command_link_pending_valid = false;
    command_link_retries = 0U;
    command_link_result_head = 0U;
    command_link_result_count = 0U;
    printf("[CMDLINK] INIT OK\r\n");
    return true;
}

bool CommandLink_Submit(const ProtocolLoraFrame *command)
{
    if ((command == NULL) || (command->type != PROTOCOL_LORA_FRAME_COMMAND))
    {
        return false;
    }
    if (command_link_pending_valid)
    {
        printf("[CMDLINK] BUSY\r\n");
        return false;
    }
    command_link_pending = *command;
    {
        ProtocolLoraCommand parsed;
        if (ProtocolLora_GetCommandPayload(command, &parsed) !=
            PROTOCOL_LORA_OK)
        {
            return false;
        }
        command_link_pending_id = parsed.command_id;
        command_link_pending_actuator = parsed.actuator;
    }
    command_link_pending_valid = true;
    command_link_retries = 0U;
    command_link_state = COMMAND_LINK_IDLE;
    printf("[CMDLINK] QUEUE ID=%u\r\n",
           (unsigned int)command_link_pending_id);
    return true;
}

static bool CommandLinkTrySend(void)
{
    if (LLCC68_P2P_IsTxPending())
    {
        return false;
    }
    if (!LLCC68_P2P_QueueFrame(&command_link_pending))
    {
        return false;
    }
    command_link_state = COMMAND_LINK_WAIT_ACK;
    command_link_sent_ms = ServiceHal_GetTickMs();
    printf("[CMDLINK] SEND ID=%u RETRY=%u\r\n",
           (unsigned int)command_link_pending_id,
           (unsigned int)command_link_retries);
    return true;
}

void CommandLink_Process(void)
{
    uint32_t now;
    CommandLinkResult result;

    if (!command_link_pending_valid)
    {
        return;
    }
    now = ServiceHal_GetTickMs();

    if (command_link_state == COMMAND_LINK_IDLE)
    {
        (void)CommandLinkTrySend();
        return;
    }

    if (((int32_t)(now - command_link_sent_ms)) <
        (int32_t)GATEWAY_COMMAND_TIMEOUT_MS)
    {
        return;
    }

    if (command_link_retries < GATEWAY_COMMAND_MAX_RETRIES)
    {
        ++command_link_retries;
        command_link_state = COMMAND_LINK_IDLE;
        printf("[CMDLINK] TIMEOUT ID=%u RETRY=%u\r\n",
               (unsigned int)command_link_pending_id,
               (unsigned int)command_link_retries);
        (void)CommandLinkTrySend();
        return;
    }

    memset(&result, 0, sizeof(result));
    result.type = COMMAND_LINK_RESULT_TIMEOUT;
    result.command_id = command_link_pending_id;
    result.actuator = command_link_pending_actuator;
    command_link_pending_valid = false;
    command_link_state = COMMAND_LINK_IDLE;
    printf("[CMDLINK] GIVE UP ID=%u\r\n",
           (unsigned int)command_link_pending_id);
    CommandLinkPushResult(&result);
}

void CommandLink_OnAckFrame(const ProtocolLoraFrame *frame)
{
    ProtocolLoraAck ack;
    CommandLinkResult result;

    if (!command_link_pending_valid || (frame == NULL) ||
        (frame->type != PROTOCOL_LORA_FRAME_ACK))
    {
        return;
    }
    if (ProtocolLora_GetAckPayload(frame, &ack) != PROTOCOL_LORA_OK)
    {
        return;
    }
    if (ack.command_id != command_link_pending_id)
    {
        printf("[CMDLINK] ACK MISMATCH ID=%u EXPECT=%u\r\n",
               (unsigned int)ack.command_id,
               (unsigned int)command_link_pending_id);
        return;
    }
    /* result 仅允许 0(OK)/1(INVALID)：拒绝异协议/异版本固件的 ACK */
    if (ack.result > PROTOCOL_LORA_ACK_INVALID)
    {
        printf("[CMDLINK] ACK BAD RESULT=%u IGNORE\r\n",
               (unsigned int)ack.result);
        return;
    }

    memset(&result, 0, sizeof(result));
    result.type = COMMAND_LINK_RESULT_ACK;
    result.command_id = ack.command_id;
    result.actuator = command_link_pending_actuator;
    result.ack = ack;
    command_link_pending_valid = false;
    command_link_state = COMMAND_LINK_IDLE;
    printf("[CMDLINK] ACK ID=%u RESULT=%u ACTUAL=%u\r\n",
           (unsigned int)ack.command_id, (unsigned int)ack.result,
           (unsigned int)ack.actual_value);
    CommandLinkPushResult(&result);
}

bool CommandLink_TakeResult(CommandLinkResult *result)
{
    if ((result == NULL) || (command_link_result_count == 0U))
    {
        return false;
    }
    *result = command_link_results[command_link_result_head];
    command_link_result_head =
        (command_link_result_head + 1U) % COMMAND_LINK_RESULT_QUEUE_SIZE;
    --command_link_result_count;
    return true;
}

bool CommandLink_IsBusy(void)
{
    return command_link_pending_valid;
}
