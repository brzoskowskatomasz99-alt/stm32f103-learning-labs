#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define __USART_H__
#define __MAIN_H

typedef int HAL_StatusTypeDef;
typedef struct { int unused; } UART_HandleTypeDef;

#define HAL_OK 0

uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t delay);

static const uint8_t *test_chunks[4];
static int32_t test_chunk_lengths[4];
static size_t test_chunk_count;
static size_t test_chunk_index;
static uint8_t test_sent[64];
static uint16_t test_sent_length;

#include "../../Core/Src/mqtt.c"

uint32_t HAL_GetTick(void) { return 0U; }
void HAL_Delay(uint32_t delay) { (void)delay; }

HAL_StatusTypeDef ESP_Send_data_len(const uint8_t *data, uint16_t len,
                                    uint16_t timeout)
{
    (void)timeout;
    memcpy(test_sent, data, len);
    test_sent_length = len;
    return HAL_OK;
}

int32_t ESP_Get_Receive_Data(uint8_t *recv_buf, int32_t *recv_len,
                             uint32_t timeout)
{
    int32_t length;
    (void)timeout;
    if (test_chunk_index >= test_chunk_count)
        return -1;
    length = test_chunk_lengths[test_chunk_index];
    if (length > *recv_len)
        length = *recv_len;
    memcpy(recv_buf, test_chunks[test_chunk_index], (size_t)length);
    *recv_len = length;
    test_chunk_index++;
    return 1000;
}

int8_t ESP_Init(void) { return -1; }
char *mqtt_parse_msg_stub(char *topic, uint8_t *msg, uint32_t msg_len)
{ (void)topic; (void)msg; (void)msg_len; return NULL; }
bool LLCC68_P2P_TakeReceivedFrameWithMeta(ProtocolLoraFrame *frame,
                                           LLCC68P2PRxMeta *meta)
{ (void)frame; (void)meta; return false; }
bool LLCC68_P2P_QueueFrame(const ProtocolLoraFrame *frame)
{ (void)frame; return false; }
BridgeMqttStatus BridgeMqtt_FormatTelemetryJson(const ProtocolLoraFrame *frame,
    int8_t rssi, char *output, size_t capacity, size_t *length)
{ (void)frame; (void)rssi; (void)output; (void)capacity; (void)length; return BRIDGE_MQTT_ERROR_FRAME; }
BridgeMqttStatus BridgeMqtt_FormatBemfaSensor(const ProtocolLoraFrame *frame,
    char *output, size_t capacity, size_t *length)
{ (void)frame; (void)output; (void)capacity; (void)length; return BRIDGE_MQTT_ERROR_FRAME; }
BridgeMqttStatus BridgeMqtt_ParseLedCommandJson(const char *json,
    ProtocolLoraFrame *frame)
{ (void)json; (void)frame; return BRIDGE_MQTT_ERROR_PAYLOAD; }
BridgeMqttStatus BridgeMqtt_FormatAckJson(const ProtocolLoraFrame *frame,
    char *output, size_t capacity, size_t *length)
{ (void)frame; (void)output; (void)capacity; (void)length; return BRIDGE_MQTT_ERROR_FRAME; }
void Light_Led2_SetBrightness(uint8_t percent) { (void)percent; }
void Light_Led2_Off(void) {}

/* M3 扩展后的新依赖桩 */
bool LLCC68_P2P_IsTxPending(void) { return false; }
void CommandLink_OnAckFrame(const ProtocolLoraFrame *frame) { (void)frame; }
bool CommandLink_TakeResult(CommandLinkResult *result)
{ (void)result; return false; }
bool CommandLink_Submit(const ProtocolLoraFrame *command)
{ (void)command; return true; }
void GatewayData_NoteTelemetry(const ProtocolLoraFrame *frame,
                               int8_t rssi, int8_t snr)
{ (void)frame; (void)rssi; (void)snr; }
void GatewayData_NoteAck(uint8_t actuator, const ProtocolLoraAck *ack)
{ (void)actuator; (void)ack; }
void GatewayData_NoteMqttState(uint8_t connected) { (void)connected; }
void TerminalTable_NoteTelemetry(uint16_t id, uint32_t now)
{ (void)id; (void)now; }
bool TerminalTable_GetEntry(uint8_t index, uint16_t *id, bool *online,
                            uint32_t now)
{ (void)index; (void)id; (void)online; (void)now; return false; }
void AlarmRegistry_OnAlarmFrame(const ProtocolLoraFrame *frame)
{ (void)frame; }
uint8_t LinkStats_GetSuccessRatePercent(void) { return 100U; }
void LinkStats_NoteTelemetry(void) {}
BridgeMqttStatus BridgeMqtt_FormatAlarmJson(const ProtocolLoraFrame *frame,
    char *output, size_t capacity, size_t *length)
{ (void)frame; (void)output; (void)capacity; (void)length;
  return BRIDGE_MQTT_ERROR_FRAME; }
BridgeMqttStatus BridgeMqtt_FormatAckJsonFromAck(const ProtocolLoraAck *ack,
    char *output, size_t capacity, size_t *length)
{ (void)ack; (void)output; (void)capacity; (void)length;
  return BRIDGE_MQTT_ERROR_ARGUMENT; }
BridgeMqttStatus BridgeMqtt_FormatErrorAckJson(uint16_t command_id,
    char *output, size_t capacity, size_t *length)
{ (void)command_id; (void)output; (void)capacity; (void)length;
  return BRIDGE_MQTT_ERROR_ARGUMENT; }
BridgeMqttStatus BridgeMqtt_ParseCommandJson(const char *json,
    ProtocolLoraFrame *frame, uint16_t *command_id)
{ (void)json; (void)frame; (void)command_id; return BRIDGE_MQTT_ERROR_PAYLOAD; }
BridgeMqttStatus BridgeMqtt_FormatStatusJson(uint8_t mqtt_connected,
    uint8_t link_rate, const uint16_t *ids, const bool *online,
    uint8_t count, char *output, size_t capacity, size_t *length)
{ (void)mqtt_connected; (void)link_rate; (void)ids; (void)online;
  (void)count; (void)output; (void)capacity; (void)length;
  return BRIDGE_MQTT_ERROR_ARGUMENT; }

#define CHECK(condition) do { if (!(condition)) { \
    printf("FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

static void set_chunks(const uint8_t **chunks, const int32_t *lengths,
                       size_t count)
{
    size_t index;
    test_chunk_count = count;
    test_chunk_index = 0U;
    test_sent_length = 0U;
    for (index = 0U; index < count; ++index)
    {
        test_chunks[index] = chunks[index];
        test_chunk_lengths[index] = lengths[index];
    }
}

int main(void)
{
    static const uint8_t part1[] = {0x90U, 0x03U};
    static const uint8_t part2[] = {0x00U, 0x0AU, 0x00U};
    const uint8_t *chunks[] = {part1, part2};
    const int32_t lengths[] = {2, 3};
    static const uint8_t expected[] = {
        0x82U, 0x0CU, 0x00U, 0x0AU, 0x00U, 0x07U,
        'a', 'g', 'r', 'i', 'c', 'm', 'd', 0x00U
    };

    set_chunks(chunks, lengths, 2U);
    CHECK(mqtt_subscribe_QoS0("agricmd") == 0);
    CHECK(test_sent_length == sizeof(expected));
    CHECK(memcmp(test_sent, expected, sizeof(expected)) == 0);
    puts("PASS mqtt_subscribe fragmented SUBACK");
    return 0;
}
