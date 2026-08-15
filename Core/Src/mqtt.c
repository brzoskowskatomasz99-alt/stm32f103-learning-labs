/**
 * @file mqtt.c
 * @brief MQTT QoS0 协议连接巴法云（移植自课件 02-MQTT协议连接巴法云 实操代码）
 * @date 2026-08-07
 *
 * 依赖 esp.c 的 ESP_Send_data_len / ESP_Get_Receive_Data；
 * ESP_Init() 成功后已进入透传模式，本模块报文经透传通道直接发给 bemfa.com:9501。
 */
#include "mqtt.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "esp.h"
#include "bridge_mqtt.h"
#include "command_link.h"
#include "llcc68_p2p.h"
#include "light.h"
#include "terminal_table.h"
#include "link_stats.h"
#include "gateway_config.h"
#include "alarm_registry.h"
#include "gateway_data.h"
#include <stdlib.h>

uint8_t mqtt_buffer[MQTT_TX_BUFFER_SIZE] = {0};

/**
 * @brief MQTT连接报文（CONNECT，QoS 0）
 * @param client_id 客户端ID(用户私钥)
 * @param user_name 用户名（巴法云填 NULL）
 * @param password  密码（巴法云填 NULL）
 * @return int32_t 0成功 -1失败
 */
int32_t mqtt_connect_QoS0(char *client_id, char *user_name, char *password)
{
    if (client_id == NULL)
        return -1;

    uint32_t client_id_len = strlen(client_id);
    uint32_t user_name_len = (user_name == NULL ? 0 : strlen(user_name));
    uint32_t password_len = (password == NULL ? 0 : strlen(password));
    uint8_t encodedByte = 0;
    uint32_t data_len;
    uint32_t mqtt_tx_len = 0;
    int32_t recv_len = MQTT_RX_BUFFER_SIZE;

    /* ---------------- 固定报头 ---------------- */
    mqtt_buffer[mqtt_tx_len++] = (1 << 4); /* CONNECT */

    /* 剩余长度(不含固定报头) = 可变报头 + 有效载荷 */
    data_len = 10 + (client_id_len + 2) +
               (user_name_len ? user_name_len + 2 : user_name_len) +
               (password_len ? password_len + 2 : password_len);
    do
    {
        encodedByte = data_len % 128;
        data_len = data_len / 128;
        if (data_len > 0)
            encodedByte |= (1 << 7);
        mqtt_buffer[mqtt_tx_len++] = encodedByte;
    } while (data_len > 0);

    /* ---------------- 可变报头 ---------------- */
    mqtt_buffer[mqtt_tx_len++] = 0;   /* 协议名长度 MSB */
    mqtt_buffer[mqtt_tx_len++] = 4;   /* 协议名长度 LSB */
    mqtt_buffer[mqtt_tx_len++] = 'M';
    mqtt_buffer[mqtt_tx_len++] = 'Q';
    mqtt_buffer[mqtt_tx_len++] = 'T';
    mqtt_buffer[mqtt_tx_len++] = 'T';
    mqtt_buffer[mqtt_tx_len++] = 4;   /* MQTT 协议版本 4 (3.1.1) */

    /* 连接标志：按实际传入置用户名/密码位，clean session=1 */
    mqtt_buffer[mqtt_tx_len++] = ((user_name_len ? 1 : 0) << 7) |
                                 ((password_len ? 1 : 0) << 6) |
                                 (1 << 1);
    mqtt_buffer[mqtt_tx_len++] = 0;   /* 心跳 60s MSB */
    mqtt_buffer[mqtt_tx_len++] = 60;  /* 心跳 60s LSB */

    /* ---------------- 有效载荷 ---------------- */
    mqtt_buffer[mqtt_tx_len++] = BYTE1(client_id_len);
    mqtt_buffer[mqtt_tx_len++] = BYTE0(client_id_len);
    memcpy(&mqtt_buffer[mqtt_tx_len], client_id, client_id_len);
    mqtt_tx_len += client_id_len;

    if (user_name_len > 0)
    {
        mqtt_buffer[mqtt_tx_len++] = BYTE1(user_name_len);
        mqtt_buffer[mqtt_tx_len++] = BYTE0(user_name_len);
        memcpy(&mqtt_buffer[mqtt_tx_len], user_name, user_name_len);
        mqtt_tx_len += user_name_len;
    }
    if (password_len > 0)
    {
        mqtt_buffer[mqtt_tx_len++] = BYTE1(password_len);
        mqtt_buffer[mqtt_tx_len++] = BYTE0(password_len);
        memcpy(&mqtt_buffer[mqtt_tx_len], password, password_len);
        mqtt_tx_len += password_len;
    }

    ESP_Send_data_len(mqtt_buffer, mqtt_tx_len, 1000);

    if (ESP_Get_Receive_Data(mqtt_buffer, &recv_len, 2000) < 0)
        return -1;

    /* 连接成功应返回 CONNACK: 20 02 ?? 00，byte[3] 为返回码 */
    if (recv_len >= 4 && mqtt_buffer[0] == 0x20 && mqtt_buffer[1] == 0x02)
    {
        if (mqtt_buffer[3] == 0x00)
        {
            printf("MQTT CONNACK OK\r\n");
            return 0;
        }
        printf("MQTT CONNACK code:%#x\r\n", mqtt_buffer[3]);
        return -1;
    }
    return -1;
}

/**
 * @brief MQTT订阅报文（SUBSCRIBE，QoS 0）
 * @param topic 订阅主题
 * @return int32_t 0成功 -1失败
 */
int32_t mqtt_subscribe_QoS0(char *topic)
{
    if (topic == NULL)
        return -1;

    uint32_t topic_len = strlen(topic);
    uint8_t encodedByte = 0;
    uint32_t data_len;
    uint32_t mqtt_tx_len = 0;
    uint16_t pack_flag = 10; /* 报文标识符 */
    uint8_t suback_buffer[16];
    uint32_t suback_length = 0U;
    int32_t remaining_timeout = 2000;

    /* ---------------- 固定报头 ---------------- */
    mqtt_buffer[mqtt_tx_len++] = (1 << 7) | (1 << 1); /* SUBSCRIBE */

    data_len = 2 + (topic_len + 2) + 1;
    do
    {
        encodedByte = data_len % 128;
        data_len = data_len / 128;
        if (data_len > 0)
            encodedByte |= (1 << 7);
        mqtt_buffer[mqtt_tx_len++] = encodedByte;
    } while (data_len > 0);

    /* ---------------- 可变报头 ---------------- */
    mqtt_buffer[mqtt_tx_len++] = BYTE1(pack_flag);
    mqtt_buffer[mqtt_tx_len++] = BYTE0(pack_flag);

    /* ---------------- 有效载荷 ---------------- */
    mqtt_buffer[mqtt_tx_len++] = BYTE1(topic_len);
    mqtt_buffer[mqtt_tx_len++] = BYTE0(topic_len);
    memcpy(&mqtt_buffer[mqtt_tx_len], topic, topic_len);
    mqtt_tx_len += topic_len;
    mqtt_buffer[mqtt_tx_len++] = 0; /* QoS 0 */

    if (ESP_Send_data_len(mqtt_buffer, mqtt_tx_len, 1000) != HAL_OK)
    {
        printf("[MQTT][SUB] send fail topic=%s\r\n", topic);
        return -1;
    }

    printf("[MQTT][SUB] waiting topic=%s\r\n", topic);

    /* 订阅成功应返回 SUBACK: 90 03 00 0A 00 */
    while ((remaining_timeout >= 0) && (suback_length < sizeof(suback_buffer)))
    {
        int32_t recv_len = (int32_t)(sizeof(suback_buffer) - suback_length);
        uint32_t index;

        remaining_timeout = ESP_Get_Receive_Data(&suback_buffer[suback_length],
                                                  &recv_len,
                                                  (uint32_t)remaining_timeout);
        if ((remaining_timeout < 0) || (recv_len <= 0))
            break;
        suback_length += (uint32_t)recv_len;

        for (index = 0U; index + 5U <= suback_length; ++index)
        {
            if ((suback_buffer[index] == 0x90U) &&
                (suback_buffer[index + 1U] == 0x03U) &&
                (pack_flag == (uint16_t)((suback_buffer[index + 2U] << 8) |
                                         suback_buffer[index + 3U])))
            {
                if (suback_buffer[index + 4U] == 0x00U)
                {
                    printf("MQTT SUBACK OK\r\n");
                    return 0;
                }
                printf("MQTT SUBACK code:%#x\r\n",
                       suback_buffer[index + 4U]);
                return -1;
            }
        }
    }
    printf("[MQTT][SUB] timeout bytes=%u\r\n",
           (unsigned int)suback_length);
    return -1;
}

/**
 * @brief MQTT发布报文（PUBLISH，QoS 0，无应答）
 * @param topic 发布主题
 * @param msg 发布消息
 * @return int32_t 0成功 -1失败
 */
int32_t mqtt_publish_QoS0(const char *topic, const char *msg)
{
    uint32_t topic_len;
    uint32_t msg_len;
    uint32_t remaining_length;
    uint32_t encoded_length;
    uint32_t mqtt_tx_len;
    uint32_t value;

    if (topic == NULL || msg == NULL)
        return -1;

    topic_len = strlen(topic);
    msg_len = strlen(msg);
    if (topic_len > 0xFFFFU)
        return -1;

    remaining_length = 2U + topic_len + msg_len;
    encoded_length = 1U;
    value = remaining_length;
    while (value >= 128U)
    {
        value /= 128U;
        encoded_length++;
    }
    if ((1U + encoded_length + remaining_length) > MQTT_TX_BUFFER_SIZE)
        return -1;

    mqtt_tx_len = 0U;

    /* ---------------- 固定报头 ---------------- */
    mqtt_buffer[mqtt_tx_len++] = (1 << 4) | (1 << 5); /* PUBLISH, QoS0, DUP0 */

    value = remaining_length;
    do
    {
        uint8_t encoded_byte = (uint8_t)(value % 128U);
        value /= 128U;
        if (value > 0U)
            encoded_byte |= 0x80U;
        mqtt_buffer[mqtt_tx_len++] = encoded_byte;
    } while (value > 0U);

    /* ---------------- 可变报头 ---------------- */
    mqtt_buffer[mqtt_tx_len++] = BYTE1(topic_len);
    mqtt_buffer[mqtt_tx_len++] = BYTE0(topic_len);
    memcpy(&mqtt_buffer[mqtt_tx_len], topic, topic_len);
    mqtt_tx_len += topic_len;

    /* ---------------- 有效载荷 ---------------- */
    memcpy(&mqtt_buffer[mqtt_tx_len], msg, msg_len);
    mqtt_tx_len += msg_len;

    return (ESP_Send_data_len(mqtt_buffer, (uint16_t)mqtt_tx_len, 1000U) == HAL_OK)
               ? 0
               : -1;
}

/**
 * @brief 解析服务器下发的 PUBLISH 消息
 * @param topic 期望的主题
 * @param msg 接收缓冲区
 * @param msg_len 实际接收长度
 * @return char* 有效载荷指针，失败返回 NULL
 */
char *mqtt_parse_msg(char *topic, uint8_t *msg, uint32_t msg_len)
{
    if (topic == NULL || msg == NULL || msg_len <= 2)
        return NULL;

    uint32_t index = 0;
    uint32_t remaining = 0;
    uint32_t multiplier = 1;
    uint16_t topic_len;
    uint8_t encodedByte;
    uint32_t payload_len;

    /* ---------------- 固定报头 ---------------- */
    if (msg[index++] != ((1 << 4) | (1 << 5))) /* 必须是 PUBLISH */
        return NULL;

    /* 剩余长度：变长编码，低字节在前 */
    for (uint8_t i = 0; i < 4; i++)
    {
        if (index >= msg_len)
            return NULL;
        encodedByte = msg[index++];
        remaining += (encodedByte & 0x7F) * multiplier;
        if ((encodedByte & 0x80) == 0)
            break;
        multiplier *= 128;
    }
    if (remaining <= 2 || remaining > (msg_len - index))
        return NULL;

    /* ---------------- 可变报头 ---------------- */
    if (index + 2 > msg_len)
        return NULL;
    topic_len = (uint16_t)((msg[index] << 8) | msg[index + 1]);
    index += 2;
    if (topic_len == 0 || (topic_len + 2) > remaining)
        return NULL;
    if (strncmp((const char *)&msg[index], topic, topic_len) != 0)
        return NULL;
    index += topic_len;

    payload_len = remaining - (topic_len + 2);
    if (payload_len == 0)
        return NULL;
    return (char *)&msg[index];
}

/**
 * @brief MQTT心跳包（PINGREQ）
 * @return int32_t 0成功 -1失败
 */
int32_t mqtt_heart_beat(void)
{
    mqtt_buffer[0] = (1 << 7) | (1 << 6); /* PINGREQ */
    mqtt_buffer[1] = 0;                   /* 剩余长度 */
    int32_t recv_len = MQTT_RX_BUFFER_SIZE;

    ESP_Send_data_len(mqtt_buffer, 2, 1000);
    if (ESP_Get_Receive_Data(mqtt_buffer, &recv_len, 3000) < 0)
        return -1;

    /* 心跳响应应返回 PINGRESP: D0 00 */
    if (recv_len < 2 || mqtt_buffer[0] != 0xD0 || mqtt_buffer[1] != 0x00)
        return -1;
    return 0;
}

/* ---------------- 集成到 main 循环 ---------------- */

static uint32_t mqtt_heart_tick = 0;
static uint32_t mqtt_status_tick = 0;
static uint8_t mqtt_connected = 0;
static ProtocolLoraFrame mqtt_received_frame;
static LLCC68P2PRxMeta mqtt_received_meta;
static char mqtt_telemetry_json[BRIDGE_MQTT_JSON_BUFFER_SIZE];
static char mqtt_bemfa_sensor[BRIDGE_MQTT_BEMFA_SENSOR_BUFFER_SIZE];
static char mqtt_ack_json[BRIDGE_MQTT_ACK_JSON_BUFFER_SIZE];
static char mqtt_status_json[BRIDGE_MQTT_STATUS_JSON_BUFFER_SIZE];
static uint8_t mqtt_rx_buffer[MQTT_RX_BUFFER_SIZE];

/**
 * @brief MQTT 初始化：连接巴法云并订阅主题（ESP_Init 成功后调用）
 */
void mqtt_init(void)
{
    uint8_t subscribe_attempt;

    mqtt_heart_tick = HAL_GetTick();
    mqtt_status_tick = HAL_GetTick();
    mqtt_connected = 0;

    printf("MQTT connecting bemfa...\r\n");
    if (mqtt_connect_QoS0(BEMFA_UID, NULL, NULL) == 0)
    {
        mqtt_connected = 1;
        GatewayData_NoteMqttState(1U);
        printf("MQTT telemetry mode ready\r\n");
        for (subscribe_attempt = 1U; subscribe_attempt <= 3U;
             ++subscribe_attempt)
        {
            printf("[MQTT][SUB] attempt=%u\r\n",
                   (unsigned int)subscribe_attempt);
            if (mqtt_subscribe_QoS0(MQTT_TOPIC_COMMAND) == 0)
            {
                printf("MQTT control topic ready\r\n");
                return;
            }
            HAL_Delay(200U);
        }
        printf("MQTT control topic unavailable\r\n");
    }
    else
    {
        GatewayData_NoteMqttState(0U);
        printf("MQTT connect fail\r\n");
    }
}

uint8_t mqtt_is_connected(void)
{
    return mqtt_connected;
}

/**
 * @brief MQTT 任务：每 20 秒发布测试消息、每 60 秒心跳、非阻塞解析下发消息
 *        与 LLCC68_P2P_Process() 一起放在 main while(1) 中
 */
void mqtt_task_loop(void)
{
    uint32_t now;
    int32_t rx_len;
    char *p;
    size_t json_length;
    size_t sensor_length;
    BridgeMqttStatus json_status;
    BridgeMqttStatus sensor_status;

    now = HAL_GetTick();

    /* 每 20 秒发布一次（首次立即发布） */
    if (LLCC68_P2P_TakeReceivedFrameWithMeta(&mqtt_received_frame,
                                              &mqtt_received_meta))
    {
        if (mqtt_received_frame.type == PROTOCOL_LORA_FRAME_ACK)
        {
            /* ACK 交给命令链路匹配（超时重发与去重），断线时也工作 */
            CommandLink_OnAckFrame(&mqtt_received_frame);
        }
        else if (mqtt_received_frame.type == PROTOCOL_LORA_FRAME_ALARM)
        {
            /* 告警登记与 OLED 报警页不依赖 MQTT 连接 */
            AlarmRegistry_OnAlarmFrame(&mqtt_received_frame);
            if (mqtt_connected &&
                (BridgeMqtt_FormatAlarmJson(&mqtt_received_frame,
                                            mqtt_ack_json,
                                            sizeof(mqtt_ack_json),
                                            &json_length) == BRIDGE_MQTT_OK) &&
                (mqtt_publish_QoS0(MQTT_TOPIC_ALARM, mqtt_ack_json) == 0))
            {
                printf("MQTT alarm publish: %s\r\n", mqtt_ack_json);
            }
            else
            {
                printf("MQTT alarm publish fail\r\n");
            }
        }
        else if (!mqtt_received_meta.valid)
        {
            printf("MQTT telemetry drop: link metrics unavailable\r\n");
        }
        else
        {
            /* 有效遥测即更新在线表与 UI 数据（不依赖 MQTT 发布结果） */
            TerminalTable_NoteTelemetry(mqtt_received_frame.source_id, now);
            GatewayData_NoteTelemetry(&mqtt_received_frame,
                                      mqtt_received_meta.rssi_dbm,
                                      mqtt_received_meta.snr_db);
            LinkStats_NoteTelemetry();

            if (mqtt_connected)
            {
            json_status = BridgeMqtt_FormatTelemetryJson(
                &mqtt_received_frame,
                mqtt_received_meta.rssi_dbm,
                mqtt_telemetry_json,
                sizeof(mqtt_telemetry_json),
                &json_length);
            sensor_status = BridgeMqtt_FormatBemfaSensor(
                &mqtt_received_frame,
                mqtt_bemfa_sensor,
                sizeof(mqtt_bemfa_sensor),
                &sensor_length);
            if (json_status != BRIDGE_MQTT_OK)
            {
                printf("MQTT telemetry drop: json=%d\r\n", (int)json_status);
            }
            else if (sensor_status != BRIDGE_MQTT_OK)
            {
                printf("MQTT telemetry drop: sensor=%d\r\n", (int)sensor_status);
            }
            else
            {
                if (mqtt_publish_QoS0(MQTT_TOPIC_BEMFA_SENSOR,
                                      mqtt_bemfa_sensor) == 0)
                {
                    printf("MQTT sensor publish: %s LEN=%u\r\n",
                           mqtt_bemfa_sensor,
                           (unsigned int)sensor_length);
                }
                else
                {
                    printf("MQTT sensor publish fail\r\n");
                }

                if (mqtt_publish_QoS0(MQTT_TOPIC_TELEMETRY_JSON,
                                      mqtt_telemetry_json) == 0)
                {
                    printf("MQTT JSON publish: %s RSSI=%d SNR=%d LEN=%u\r\n",
                           mqtt_telemetry_json,
                           (int)mqtt_received_meta.rssi_dbm,
                           (int)mqtt_received_meta.snr_db,
                           (unsigned int)json_length);
                }
                else
                {
                    printf("MQTT JSON publish fail\r\n");
                }
            }
            }
        }
    }

    /* 命令链路结果 → 云端 ACK */
    {
        CommandLinkResult link_result;
        while (CommandLink_TakeResult(&link_result))
        {
            if (!mqtt_connected)
            {
                printf("MQTT drop down\r\n");
                continue;
            }
            if (link_result.type == COMMAND_LINK_RESULT_ACK)
            {
                GatewayData_NoteAck(link_result.actuator, &link_result.ack);
                if ((BridgeMqtt_FormatAckJsonFromAck(
                         &link_result.ack, mqtt_ack_json,
                         sizeof(mqtt_ack_json), &json_length) ==
                     BRIDGE_MQTT_OK) &&
                    (mqtt_publish_QoS0(MQTT_TOPIC_ACK, mqtt_ack_json) == 0))
                {
                    printf("MQTT ACK publish: %s\r\n", mqtt_ack_json);
                }
                else
                {
                    printf("MQTT ACK publish fail\r\n");
                }
            }
            else
            {
                if ((BridgeMqtt_FormatErrorAckJson(
                         link_result.command_id, mqtt_ack_json,
                         sizeof(mqtt_ack_json), &json_length) ==
                     BRIDGE_MQTT_OK) &&
                    (mqtt_publish_QoS0(MQTT_TOPIC_ACK, mqtt_ack_json) == 0))
                {
                    printf("MQTT timeout ACK publish: %s\r\n", mqtt_ack_json);
                }
                else
                {
                    printf("MQTT timeout ACK publish fail\r\n");
                }
            }
        }
    }

    /* 每 60 秒发布网关状态（含终端在线表），需连接 */
    if (!mqtt_connected)
    {
        return;
    }
    if ((now - mqtt_status_tick) >= GATEWAY_STATUS_INTERVAL_MS)
    {
        uint16_t status_ids[TERMINAL_TABLE_MAX_TERMINALS];
        bool status_online[TERMINAL_TABLE_MAX_TERMINALS];
        uint8_t status_count = 0U;
        size_t status_length = 0U;

        mqtt_status_tick = now;
        while ((status_count < TERMINAL_TABLE_MAX_TERMINALS) &&
               TerminalTable_GetEntry(status_count,
                                      &status_ids[status_count],
                                      &status_online[status_count], now))
        {
            ++status_count;
        }
        if ((BridgeMqtt_FormatStatusJson(
                 1U, LinkStats_GetSuccessRatePercent(),
                 status_ids, status_online, status_count,
                 mqtt_status_json, sizeof(mqtt_status_json),
                 &status_length) == BRIDGE_MQTT_OK) &&
            (mqtt_publish_QoS0(MQTT_TOPIC_STATUS, mqtt_status_json) == 0))
        {
            printf("MQTT status publish: %s\r\n", mqtt_status_json);
        }
        else
        {
            printf("MQTT status publish fail\r\n");
        }
    }

    /* 每 60 秒心跳 */
    if (now - mqtt_heart_tick > 60000)
    {
        mqtt_heart_tick = now;
        if (mqtt_heart_beat() != 0)
        {
            printf("MQTT heartbeat fail, reconnect\r\n");
            mqtt_connected = 0;
            GatewayData_NoteMqttState(0U);
            mqtt_init();
            return;
        }
    }

    /* 非阻塞接收并解析服务器下发消息 */
    rx_len = MQTT_RX_BUFFER_SIZE;
    memset(mqtt_rx_buffer, 0, sizeof(mqtt_rx_buffer));
    if (ESP_Get_Receive_Data(mqtt_rx_buffer, &rx_len, 0) >= 0 && rx_len > 0)
    {
        p = mqtt_parse_msg(MQTT_TOPIC_COMMAND, mqtt_rx_buffer, (uint32_t)rx_len);
        if (p != NULL)
        {
            ProtocolLoraFrame command_frame;
            uint16_t command_id = 0U;
            if (BridgeMqtt_ParseCommandJson(p, &command_frame,
                                            &command_id) != BRIDGE_MQTT_OK)
            {
                printf("MQTT command invalid: %s\r\n", p);
                if ((command_id != 0U) &&
                    (BridgeMqtt_FormatErrorAckJson(
                         command_id, mqtt_ack_json, sizeof(mqtt_ack_json),
                         &json_length) == BRIDGE_MQTT_OK) &&
                    (mqtt_publish_QoS0(MQTT_TOPIC_ACK, mqtt_ack_json) == 0))
                {
                    printf("MQTT invalid ACK publish: %s\r\n", mqtt_ack_json);
                }
            }
            else if (CommandLink_Submit(&command_frame))
            {
                printf("MQTT command queued: %s\r\n", p);
            }
            else
            {
                printf("MQTT command busy\r\n");
            }
        }
    }
}
