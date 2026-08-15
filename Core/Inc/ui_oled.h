#ifndef UI_OLED_H
#define UI_OLED_H

#include <stdbool.h>
#include <stdint.h>

/*
 * ui_oled（网关侧）：OLED 人机交互状态机（任务书第 7 章）。
 * P0 概览 / P1 环境 / P2 执行 / P3 链路 / P4 报警（报警优先显示）；
 * SW1 短按下一页、SW2 短按上一页（报警页浏览上一条）、SW1 长按开关轮显、
 * SW2 长按蜂鸣器静音请求、5 min 无操作息屏、按键或新报警唤醒。
 * 页面渲染经 ui_oled_hal.h 抽象，主机可单测。
 */

typedef struct
{
    bool valid; /* 是否有有效遥测 */
    int16_t temperature_x10;
    uint16_t humidity_x10;
    uint16_t co2_ppm;
    uint16_t lux;
    uint16_t soil_x10;
    int8_t rssi_dbm;
    int8_t snr_db;
} UiOledTelemetry;

typedef struct
{
    uint8_t mqtt_connected;
    uint8_t link_rate_percent;
    uint8_t terminal_count;
    uint8_t terminal_online_count;
    uint8_t actuator_valid[5]; /* 1 LED 2 蜂鸣器 3 继电器 4 灯光 5 风机 */
    uint8_t actuator_value[5]; /* 0..100 */
} UiOledStatus;

bool UiOled_Init(void);
void UiOled_SetData(const UiOledTelemetry *telemetry,
                    const UiOledStatus *status);
/* 每个主循环节拍调用（内部取 ServiceHal_GetTickMs） */
void UiOled_Process(void);
/* SW2 长按静音请求（一次性，取后清除） */
bool UiOled_GetSilenceRequest(void);

#endif /* UI_OLED_H */
