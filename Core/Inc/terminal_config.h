#ifndef TERMINAL_CONFIG_H
#define TERMINAL_CONFIG_H

/*
 * 终端自治配置模块（任务书第 6 章：阈值必须存放在 config 模块，
 * 支持编译期默认值）。以下为任务书第 6 章"初始工程验收建议值"。
 */

/* 控制/告警调度周期（本地阈值动作延迟 <= 3 s：500 ms 节拍 + 1.5 s 确认） */
#define TERMINAL_CONTROL_TICK_MS         500U

/* ---- 光照联动 LIGHT_LOW：lux < 300 触发 / lux >= 350 恢复 ----
   GL5528 经 light.c 查表后亮光下饱和为 350 lux，恢复阈值必须可达（>=350） */
#define TERMINAL_LIGHT_LOW_ON_LUX        300U
#define TERMINAL_LIGHT_LOW_OFF_LUX       350U
#define TERMINAL_AUTO_LIGHT_PERCENT      100U

/* ---- 土壤联动 SOIL_WET（x10）：soil > 80% 触发 / < 75% 恢复 ---- */
#define TERMINAL_SOIL_WET_ON_X10         800U
#define TERMINAL_SOIL_WET_OFF_X10        750U

/* ---- 土壤联动 SOIL_DRY（x10）：soil < 40% 触发 / > 45% 恢复 ---- */
#define TERMINAL_SOIL_DRY_ON_X10         400U
#define TERMINAL_SOIL_DRY_OFF_X10        450U

/* ---- CO2 联动 CO2_HIGH：> 1500 ppm 触发 / < 1300 恢复，> 2000 升级告警 ---- */
#define TERMINAL_CO2_HIGH_ON_PPM         1500U
#define TERMINAL_CO2_HIGH_OFF_PPM        1300U
#define TERMINAL_CO2_DANGER_PPM          2000U
#define TERMINAL_AUTO_FAN_PERCENT        80U

/* ---- 温湿度告警（x10）：< 10 或 > 35 ℃、> 90 %（温度阈值为有符号值） ---- */
#define TERMINAL_TEMP_LOW_ON_X10         100
#define TERMINAL_TEMP_LOW_OFF_X10        110
#define TERMINAL_TEMP_HIGH_ON_X10        350
#define TERMINAL_TEMP_HIGH_OFF_X10       340
#define TERMINAL_HUMI_HIGH_ON_X10        900U
#define TERMINAL_HUMI_HIGH_OFF_X10       880U

/* ---- 延时确认 ---- */
#define TERMINAL_RULE_CONFIRM_MS         1500U
#define TERMINAL_BEEP_CONFIRM_MS         6000U

/* ---- 传感器故障判定：连续 3 个采集周期失败 ---- */
#define TERMINAL_SENSOR_FAIL_COUNT       3U
#define TERMINAL_SENSOR_FAULT_REPORT_MS  60000U
#define TERMINAL_CO2_MAX_PPM             5000U
#define TERMINAL_FILTER_WINDOW           3U

/* ---- P3 远程手动有效期：30 min，超时自动恢复本地自动 ---- */
#define TERMINAL_MANUAL_VALIDITY_MS      1800000U

#endif /* TERMINAL_CONFIG_H */
