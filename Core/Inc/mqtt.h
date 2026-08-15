#ifndef __MQTT_H
#define __MQTT_H

#include "main.h"
#include "esp.h"
#include "secrets.h"

#define BYTE0(dwTemp) (*(char *)(&dwTemp))
#define BYTE1(dwTemp) (*((char *)(&dwTemp) + 1))
#define BYTE2(dwTemp) (*((char *)(&dwTemp) + 2))
#define BYTE3(dwTemp) (*((char *)(&dwTemp) + 3))

#define MQTT_RX_BUFFER_SIZE ESP_RX_BUF_SIZE
#define MQTT_TX_BUFFER_SIZE 192U

/* /up updates the BaFa cloud value without forwarding to subscribers. */
#define MQTT_TOPIC_BEMFA_SENSOR "agri004/up"
#define MQTT_TOPIC_TELEMETRY_JSON "agrijson/up"
#define MQTT_TOPIC_COMMAND "agricmd"
#define MQTT_TOPIC_ACK "agriack/up"
#define MQTT_TOPIC_ALARM "agrialarm/up"
#define MQTT_TOPIC_STATUS "agristatus/up"

/* Active BaFa account configuration. Keep this private key out of reports.
   BEMFA_UID 位于 secrets.h（不入库）。 */
#define BEMFA_TOPIC_TEMP "temp004"
#define BEMFA_TOPIC_LED "led002"

extern uint8_t mqtt_buffer[MQTT_TX_BUFFER_SIZE];

int32_t mqtt_connect_QoS0(char *client_id, char *user_name, char *password);
int32_t mqtt_subscribe_QoS0(char *topic);
int32_t mqtt_publish_QoS0(const char *topic, const char *msg);
char *mqtt_parse_msg(char *topic, uint8_t *msg, uint32_t msg_len);
int32_t mqtt_heart_beat(void);

void mqtt_init(void);
void mqtt_task_loop(void);
uint8_t mqtt_is_connected(void);

#endif
