/* terminal_autonomy 主机单元测试：桩实现传感器快照与 LoRa 发送。 */
#include "terminal_autonomy.h"

#include "protocol_lora.h"
#include "service_hal.h"
#include "terminal_sensors.h"

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
static TerminalSensorSnapshot g_snapshot;
static ProtocolLoraFrame g_sent_frames[8];
static uint8_t g_sent_count = 0U;
static int g_queue_busy = 0;

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

bool TerminalSensors_GetSnapshot(TerminalSensorSnapshot *snapshot)
{
    if (snapshot == NULL)
    {
        return false;
    }
    *snapshot = g_snapshot;
    return true;
}

bool LLCC68_P2P_QueueFrame(const ProtocolLoraFrame *frame)
{
    if (g_queue_busy)
    {
        return false;
    }
    if (g_sent_count < 8U)
    {
        g_sent_frames[g_sent_count++] = *frame;
    }
    return true;
}

bool LLCC68_P2P_IsTxPending(void)
{
    return false;
}

/* ---- 工具 ---- */
static void TestReset(void)
{
    g_now_ms = 0U;
    g_sent_count = 0U;
    g_queue_busy = 0;
    memset(&g_snapshot, 0, sizeof(g_snapshot));
    g_snapshot.temperature_x10 = 250;
    g_snapshot.humidity_x10 = 500;
    g_snapshot.co2_ppm = 600;
    g_snapshot.lux = 500;
    g_snapshot.soil_x10 = 500;
    g_snapshot.device_status = 0U;
}

static void Tick(void)
{
    g_now_ms += 500U;
    TerminalAutonomy_Process();
}

static int test_no_alarm_queues_nothing(void)
{
    TestReset();
    CHECK(TerminalAutonomy_Init() == true);
    Tick();
    Tick();
    Tick();
    Tick();
    CHECK(g_sent_count == 0U);
    return 0;
}

static int test_fault_raises_alarm_frame(void)
{
    ProtocolLoraFrame frame;
    ProtocolLoraAlarm alarm;

    TestReset();
    CHECK(TerminalAutonomy_Init() == true);
    g_snapshot.device_status |= TERMINAL_SENSOR_STATUS_ANY_FAULT;
    Tick();
    Tick();
    Tick();
    Tick(); /* 1.5 s 确认 */
    CHECK(g_sent_count == 1U);
    frame = g_sent_frames[0];
    CHECK(frame.type == PROTOCOL_LORA_FRAME_ALARM);
    CHECK(frame.source_id == PROTOCOL_LORA_FIRST_TERMINAL_ID);
    CHECK(frame.destination_id == PROTOCOL_LORA_GATEWAY_ID);
    CHECK(ProtocolLora_GetAlarmPayload(&frame, &alarm) == PROTOCOL_LORA_OK);
    CHECK(alarm.alarm_code == 7U); /* SENSOR_FAULT */
    CHECK(alarm.alarm_level == 2U);
    CHECK(alarm.active == 1U);
    return 0;
}

static int test_busy_queue_retains_event(void)
{
    ProtocolLoraAlarm alarm;

    TestReset();
    CHECK(TerminalAutonomy_Init() == true);
    g_snapshot.device_status |= TERMINAL_SENSOR_STATUS_ANY_FAULT;
    Tick();
    Tick();
    Tick();
    g_queue_busy = 1;
    Tick(); /* 告警成立但发送队列忙 */
    CHECK(g_sent_count == 0U);
    g_queue_busy = 0;
    Tick(); /* 下一节拍重试成功 */
    CHECK(g_sent_count == 1U);
    CHECK(ProtocolLora_GetAlarmPayload(&g_sent_frames[0], &alarm) ==
          PROTOCOL_LORA_OK);
    CHECK(alarm.active == 1U);

    /* 故障消除 → 恢复帧 */
    g_snapshot.device_status &= (uint16_t)(~TERMINAL_SENSOR_STATUS_ANY_FAULT);
    Tick();
    Tick();
    Tick();
    Tick();
    CHECK(g_sent_count == 2U);
    CHECK(ProtocolLora_GetAlarmPayload(&g_sent_frames[1], &alarm) ==
          PROTOCOL_LORA_OK);
    CHECK(alarm.active == 0U);
    return 0;
}

int main(void)
{
    CHECK(test_no_alarm_queues_nothing() == 0);
    CHECK(test_fault_raises_alarm_frame() == 0);
    CHECK(test_busy_queue_retains_event() == 0);
    puts("PASS terminal_autonomy");
    return 0;
}
