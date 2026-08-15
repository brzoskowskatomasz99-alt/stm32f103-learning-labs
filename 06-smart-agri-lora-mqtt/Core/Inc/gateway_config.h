#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H

/*
 * 网关配置模块（任务书第 8 章：配置与业务分离）。
 */

/* LoRa 命令链路：1 s 超时，最多重发 3 次（任务书第 9 章可靠性） */
#define GATEWAY_COMMAND_TIMEOUT_MS    1000U
#define GATEWAY_COMMAND_MAX_RETRIES   3U

/* 终端在线表：超过该时长未收到遥测判定离线 */
#define GATEWAY_TERMINAL_OFFLINE_MS   30000U

/* 状态主题发布周期 */
#define GATEWAY_STATUS_INTERVAL_MS    60000U

/* 链路统计：滑动窗口长度（遥测帧数） */
#define GATEWAY_LINK_STATS_WINDOW     50U

/* OLED 交互（任务书第 7 章） */
#define GATEWAY_OLED_SLEEP_TIMEOUT_MS 300000U /* 5 min 无操作息屏 */
#define GATEWAY_OLED_ROTATE_MS        5000U   /* 自动轮换周期 */
#define GATEWAY_KEY_LONG_PRESS_MS     1500U   /* 长按阈值 */

#endif /* GATEWAY_CONFIG_H */
