#include "ui_oled.h"

#include "alarm_registry.h"
#include "bridge_mqtt.h"
#include "gateway_config.h"
#include "service_hal.h"
#include "ui_oled_hal.h"

#include <stdio.h>
#include <string.h>

/*
 * ui_oled（网关侧）：OLED 人机交互状态机（任务书第 7 章）。
 * P0 概览 / P1 环境 / P2 执行 / P3 链路 / P4 报警（报警优先显示）；
 * SW1 短按下一页、SW2 短按上一页（报警页浏览上一条）、SW1 长按开关轮显、
 * SW2 长按蜂鸣器静音请求、5 min 无操作息屏、按键或新报警唤醒。
 *
 * 防闪烁：内容签名变化或每 5 s 才整帧刷新；清屏只清显存。
 * 显示内容为"字库中文 + ASCII 数字"混合（FontDotMatrix16 字库仅含
 * 中/您好/陈工/粤嵌科技/温度/湿度/光照/土壤等少量汉字）。
 */

#define UI_REFRESH_INTERVAL_MS  5000U
#define UI_KEY_DEBOUNCE_MS      20U
#define UI_LINE_PIXEL_HEIGHT    16U

typedef enum
{
    UI_PAGE_OVERVIEW = 0,
    UI_PAGE_ENV,
    UI_PAGE_ACTUATOR,
    UI_PAGE_LINK,
    UI_PAGE_ALARM,
    UI_PAGE_COUNT
} UiPage;

typedef struct
{
    uint8_t pressed;
    uint32_t press_ms;
    uint8_t long_sent;
} UiKeyState;

static UiOledTelemetry ui_telemetry;
static UiOledStatus ui_status;
static UiOledTelemetry ui_telemetry_rendered;
static UiOledStatus ui_status_rendered;
static uint8_t ui_page = UI_PAGE_OVERVIEW;
static uint8_t ui_page_rendered = 0xFFU;
static uint8_t ui_rotate_enabled = 1U;
static uint8_t ui_screen_on = 1U;
static uint8_t ui_silence_request = 0U;
static uint8_t ui_alarm_browse = 0U;
static uint8_t ui_alarm_browse_rendered = 0xFFU;
static uint8_t ui_prev_alarm_count = 0U;
static uint16_t ui_prev_highest_code = 0U;
static uint32_t ui_last_activity_ms = 0U;
static uint32_t ui_last_refresh_ms = 0U;
static uint32_t ui_rotate_tick_ms = 0U;
static UiKeyState ui_key1;
static UiKeyState ui_key2;

static const char *UiActuatorValue(const UiOledStatus *status,
                                   uint8_t index,
                                   char *buffer,
                                   size_t capacity)
{
    if (status->actuator_valid[index] != 0U)
    {
        (void)snprintf(buffer, capacity, "%u",
                       (unsigned int)status->actuator_value[index]);
    }
    else
    {
        (void)snprintf(buffer, capacity, "--");
    }
    return buffer;
}

static void UiFormatTenths(int16_t value_x10, char *output, size_t capacity)
{
    uint16_t magnitude;

    if (value_x10 < 0)
    {
        magnitude = (uint16_t)(-value_x10);
        (void)snprintf(output, capacity, "-%u.%u",
                       (unsigned int)(magnitude / 10U),
                       (unsigned int)(magnitude % 10U));
    }
    else
    {
        magnitude = (uint16_t)value_x10;
        (void)snprintf(output, capacity, "%u.%u",
                       (unsigned int)(magnitude / 10U),
                       (unsigned int)(magnitude % 10U));
    }
}

/* 每行最多 8 个 16px 字形（128/16），超长会被驱动截断 */
static void UiDrawLine(uint8_t line, const char *text)
{
    UiHal_DrawText(0U, (uint8_t)(line * UI_LINE_PIXEL_HEIGHT), text);
}

static void UiDrawPage(uint8_t page)
{
    char line[24];
    char value_a[8];
    char value_b[8];

    UiHal_Clear(); /* 只清显存，不闪屏 */
    switch (page)
    {
    case UI_PAGE_OVERVIEW:
        UiDrawLine(0U, "\xE7\xB2\xA4\xE5\xB5\x8C\xE7\xA7\x91\xE6\x8A\x80"); /* 粤嵌科技 */
        (void)snprintf(line, sizeof(line), "MQTT:%s",
                       (ui_status.mqtt_connected != 0U) ? "OK" : "NO");
        UiDrawLine(1U, line);
        (void)snprintf(line, sizeof(line), "TERM:%u/%u",
                       (unsigned int)ui_status.terminal_online_count,
                       (unsigned int)ui_status.terminal_count);
        UiDrawLine(2U, line);
        (void)snprintf(line, sizeof(line), "ALARM:%u",
                       (unsigned int)AlarmRegistry_GetCount());
        UiDrawLine(3U, line);
        break;
    case UI_PAGE_ENV:
        if (ui_telemetry.valid)
        {
            char temperature[10];
            char humidity[10];

            UiFormatTenths(ui_telemetry.temperature_x10, temperature,
                           sizeof(temperature));
            UiFormatTenths((int16_t)ui_telemetry.humidity_x10, humidity,
                           sizeof(humidity));
            (void)snprintf(line, sizeof(line),
                           "\xE6\xB8\xA9\xE5\xBA\xA6:%sC", temperature); /* 温度 */
            UiDrawLine(0U, line);
            (void)snprintf(line, sizeof(line),
                           "\xE6\xB9\xBF\xE5\xBA\xA6:%s%%", humidity); /* 湿度 */
            UiDrawLine(1U, line);
            (void)snprintf(line, sizeof(line),
                           "\xE5\x85\x89\xE7\x85\xA7:%u", /* 光照 */
                           (unsigned int)ui_telemetry.lux);
            UiDrawLine(2U, line);
            UiFormatTenths((int16_t)ui_telemetry.soil_x10, humidity,
                           sizeof(humidity));
            (void)snprintf(line, sizeof(line),
                           "\xE5\x9C\x9F\xE5\xA3\xA4:%s%%", humidity); /* 土壤 */
            UiDrawLine(3U, line);
        }
        else
        {
            UiDrawLine(0U,
                       "\xE6\xB8\xA9\xE5\xBA\xA6\xE6\xB9\xBF\xE5\xBA\xA6"); /* 温度湿度 */
            UiDrawLine(1U,
                       "\xE5\x85\x89\xE7\x85\xA7\xE5\x9C\x9F\xE5\xA3\xA4"); /* 光照土壤 */
            UiDrawLine(2U, "NO DATA");
        }
        break;
    case UI_PAGE_ACTUATOR:
        (void)snprintf(line, sizeof(line), "LED:%s BE:%s",
                       UiActuatorValue(&ui_status, 0U, value_a,
                                       sizeof(value_a)),
                       UiActuatorValue(&ui_status, 1U, value_b,
                                       sizeof(value_b)));
        UiDrawLine(0U, line);
        (void)snprintf(line, sizeof(line), "RELAY:%s",
                       UiActuatorValue(&ui_status, 2U, value_a,
                                       sizeof(value_a)));
        UiDrawLine(1U, line);
        (void)snprintf(line, sizeof(line), "\xE5\x85\x89:%s%%", /* 光 */
                       UiActuatorValue(&ui_status, 3U, value_a,
                                       sizeof(value_a)));
        UiDrawLine(2U, line);
        (void)snprintf(line, sizeof(line), "FAN:%s%%",
                       UiActuatorValue(&ui_status, 4U, value_a,
                                       sizeof(value_a)));
        UiDrawLine(3U, line);
        break;
    case UI_PAGE_LINK:
        if (ui_telemetry.valid)
        {
            (void)snprintf(line, sizeof(line), "RSSI:%d",
                           (int)ui_telemetry.rssi_dbm);
        }
        else
        {
            (void)snprintf(line, sizeof(line), "RSSI:--");
        }
        UiDrawLine(0U, line);
        (void)snprintf(line, sizeof(line), "MQTT:%s",
                       (ui_status.mqtt_connected != 0U) ? "OK" : "NO");
        UiDrawLine(1U, line);
        (void)snprintf(line, sizeof(line), "RATE:%u%%",
                       (unsigned int)ui_status.link_rate_percent);
        UiDrawLine(2U, line);
        break;
    case UI_PAGE_ALARM:
    default:
    {
        uint8_t count = AlarmRegistry_GetCount();
        uint16_t code = 0U;
        uint8_t level = 0U;

        if (count == 0U)
        {
            UiDrawLine(0U, "ALARM:0");
            UiDrawLine(1U, "NO ALARM");
            break;
        }
        if (ui_alarm_browse >= count)
        {
            ui_alarm_browse = 0U;
        }
        (void)AlarmRegistry_GetEntry(ui_alarm_browse, &code, &level);
        (void)snprintf(line, sizeof(line), "ALM:%u/%u",
                       (unsigned int)(ui_alarm_browse + 1U),
                       (unsigned int)count);
        UiDrawLine(0U, line);
        (void)snprintf(line, sizeof(line), "CODE:%s",
                       BridgeMqtt_AlarmCodeString(code));
        UiDrawLine(1U, line);
        (void)snprintf(line, sizeof(line), "LEVEL:%u", (unsigned int)level);
        UiDrawLine(2U, line);
        UiDrawLine(3U, "ACTIVE");
        break;
    }
    }
    UiHal_Refresh(); /* 一次整帧推送 */
}

/* 内容签名变化才刷新，另每 UI_REFRESH_INTERVAL_MS 强制刷新一次 */
static void UiMaybeRender(uint8_t display_page)
{
    uint32_t now = ServiceHal_GetTickMs();
    uint8_t changed;

    changed = 0U;
    if ((memcmp(&ui_telemetry, &ui_telemetry_rendered,
                sizeof(ui_telemetry)) != 0) ||
        (memcmp(&ui_status, &ui_status_rendered, sizeof(ui_status)) != 0) ||
        (ui_page != ui_page_rendered) ||
        (ui_alarm_browse != ui_alarm_browse_rendered) ||
        ((now - ui_last_refresh_ms) >= UI_REFRESH_INTERVAL_MS))
    {
        changed = 1U;
    }
    if (changed == 0U)
    {
        return;
    }
    ui_telemetry_rendered = ui_telemetry;
    ui_status_rendered = ui_status;
    ui_page_rendered = display_page;
    ui_alarm_browse_rendered = ui_alarm_browse;
    ui_last_refresh_ms = now;
    UiDrawPage(display_page);
}

static void UiNextPage(void)
{
    ui_page = (uint8_t)((ui_page + 1U) % UI_PAGE_ALARM); /* P0..P3 循环 */
}

static void UiPrevPage(void)
{
    ui_page = (ui_page == UI_PAGE_OVERVIEW) ? (UI_PAGE_ALARM - 1U)
                                            : (uint8_t)(ui_page - 1U);
}

static void UiWakeAndActivity(void)
{
    if (ui_screen_on == 0U)
    {
        UiHal_SetPower(true);
        ui_screen_on = 1U;
        printf("[UI] WAKE\r\n");
        ui_page_rendered = 0xFFU; /* 强制重绘 */
    }
    ui_last_activity_ms = ServiceHal_GetTickMs();
    ui_rotate_tick_ms = ui_last_activity_ms; /* 操作后重新计时轮换 */
}

static uint8_t UiScanKey(UiKeyState *key, uint8_t down, uint32_t now,
                         uint8_t long_event, uint8_t short_event)
{
    if (key->pressed == 0U)
    {
        if (down != 0U)
        {
            key->pressed = 1U;
            key->press_ms = now;
            key->long_sent = 0U;
        }
        return 0U;
    }
    if (down != 0U)
    {
        if ((key->long_sent == 0U) &&
            ((now - key->press_ms) >= GATEWAY_KEY_LONG_PRESS_MS))
        {
            key->long_sent = 1U;
            return long_event;
        }
        return 0U;
    }
    key->pressed = 0U;
    if ((key->long_sent == 0U) &&
        ((now - key->press_ms) >= UI_KEY_DEBOUNCE_MS))
    {
        return short_event;
    }
    return 0U;
}

bool UiOled_Init(void)
{
    memset(&ui_telemetry, 0, sizeof(ui_telemetry));
    memset(&ui_status, 0, sizeof(ui_status));
    memset(&ui_telemetry_rendered, 0xFF, sizeof(ui_telemetry_rendered));
    memset(&ui_status_rendered, 0xFF, sizeof(ui_status_rendered));
    memset(&ui_key1, 0, sizeof(ui_key1));
    memset(&ui_key2, 0, sizeof(ui_key2));
    ui_page = UI_PAGE_OVERVIEW;
    ui_page_rendered = 0xFFU;
    ui_rotate_enabled = 1U;
    ui_screen_on = 1U;
    ui_silence_request = 0U;
    ui_alarm_browse = 0U;
    ui_alarm_browse_rendered = 0xFFU;
    ui_prev_alarm_count = 0U;
    ui_prev_highest_code = 0U;
    ui_last_activity_ms = ServiceHal_GetTickMs();
    ui_last_refresh_ms = 0U;
    ui_rotate_tick_ms = ui_last_activity_ms;

    UiHal_Init();
    UiHal_SetPower(true);
    printf("[UI] INIT OK\r\n");
    return true;
}

void UiOled_SetData(const UiOledTelemetry *telemetry,
                    const UiOledStatus *status)
{
    uint8_t alarm_count;
    uint16_t highest_code;

    if (telemetry != NULL)
    {
        ui_telemetry = *telemetry;
    }
    if (status != NULL)
    {
        ui_status = *status;
    }
    alarm_count = AlarmRegistry_GetCount();
    highest_code = AlarmRegistry_GetHighestCode();
    if ((alarm_count > ui_prev_alarm_count) ||
        ((highest_code != 0U) && (highest_code != ui_prev_highest_code)))
    {
        printf("[UI] NEW ALARM\r\n");
        UiWakeAndActivity();
    }
    ui_prev_alarm_count = alarm_count;
    ui_prev_highest_code = highest_code;
}

void UiOled_Process(void)
{
    uint32_t now = ServiceHal_GetTickMs();
    uint8_t event;
    uint8_t alarm_count;
    uint8_t display_page;

    /* SW1：短按下一页(1)、长按开关轮显(2)；息屏时按键仅唤醒 */
    event = UiScanKey(&ui_key1, UiHal_ReadKey1(), now, 2U, 1U);
    if (event == 2U)
    {
        if (ui_screen_on != 0U)
        {
            ui_rotate_enabled = (ui_rotate_enabled == 0U) ? 1U : 0U;
            printf("[UI] ROTATE %s\r\n",
                   (ui_rotate_enabled != 0U) ? "ON" : "OFF");
        }
        UiWakeAndActivity();
    }
    else if (event == 1U)
    {
        if (ui_screen_on != 0U)
        {
            UiNextPage();
        }
        UiWakeAndActivity();
    }

    /* SW2：短按上一页/报警浏览(3)、长按静音请求(4) */
    event = UiScanKey(&ui_key2, UiHal_ReadKey2(), now, 4U, 3U);
    if (event == 4U)
    {
        ui_silence_request = 1U;
        printf("[UI] SILENCE REQ\r\n");
        UiWakeAndActivity();
    }
    else if (event == 3U)
    {
        if (ui_screen_on != 0U)
        {
            if (AlarmRegistry_GetCount() > 0U)
            {
                ++ui_alarm_browse;
            }
            else
            {
                UiPrevPage();
            }
        }
        UiWakeAndActivity();
    }

    /* 息屏 */
    if ((ui_screen_on != 0U) &&
        ((now - ui_last_activity_ms) >= GATEWAY_OLED_SLEEP_TIMEOUT_MS))
    {
        ui_screen_on = 0U;
        UiHal_SetPower(false);
        printf("[UI] SLEEP\r\n");
    }
    if (ui_screen_on == 0U)
    {
        return;
    }

    alarm_count = AlarmRegistry_GetCount();
    if (alarm_count > 0U)
    {
        ui_alarm_browse = (ui_alarm_browse < alarm_count)
                              ? ui_alarm_browse
                              : 0U;
    }

    /* 自动轮换（仅无报警时在 P0..P3 间轮换，任务书 5 s） */
    if ((ui_rotate_enabled != 0U) && (alarm_count == 0U) &&
        ((now - ui_rotate_tick_ms) >= GATEWAY_OLED_ROTATE_MS))
    {
        ui_rotate_tick_ms = now;
        UiNextPage();
    }

    display_page = (alarm_count > 0U) ? UI_PAGE_ALARM : ui_page;
    UiMaybeRender(display_page);
}

bool UiOled_GetSilenceRequest(void)
{
    bool request = (ui_silence_request != 0U);

    ui_silence_request = 0U;
    return request;
}
