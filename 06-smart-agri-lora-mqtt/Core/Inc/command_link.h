#ifndef COMMAND_LINK_H
#define COMMAND_LINK_H

#include <stdbool.h>
#include <stdint.h>

#include "protocol_lora.h"

/*
 * command_link（网关侧）：LoRa 命令链路可靠性（任务书第 9 章）。
 * 1 s 超时、最多重发 3 次、按 command_id 匹配 ACK；重复命令不重复执行由终端
 * 侧去重保证（重发相同 command_id 不会二次执行）。
 * 结果经事件队列交给 mqtt 层发布云端 ACK。
 */

typedef enum
{
    COMMAND_LINK_RESULT_ACK = 0,
    COMMAND_LINK_RESULT_TIMEOUT
} CommandLinkResultType;

typedef struct
{
    CommandLinkResultType type;
    uint16_t command_id;
    uint8_t actuator; /* 命令对应的执行器编码（PROTOCOL_LORA_ACTUATOR_*） */
    ProtocolLoraAck ack;
} CommandLinkResult;

bool CommandLink_Init(void);

/* 提交一条待发送命令；忙时返回 false */
bool CommandLink_Submit(const ProtocolLoraFrame *command);

/* 每个主循环节拍调用：驱动发送/超时/重试 */
void CommandLink_Process(void);

/* 由 mqtt 层把收到的 ACK 帧交给链路匹配 */
void CommandLink_OnAckFrame(const ProtocolLoraFrame *frame);

bool CommandLink_TakeResult(CommandLinkResult *result);
bool CommandLink_IsBusy(void);

#endif /* COMMAND_LINK_H */
