#include "link_stats.h"

#include <stdio.h>
#include <string.h>

#define LINK_STATS_SLOT_MS 20000U /* 终端遥测上报周期 */

static uint8_t link_stats_slots[GATEWAY_LINK_STATS_WINDOW];
static uint8_t link_stats_index = 0U;
static uint8_t link_stats_filled = 0U;
static uint16_t link_stats_total = 0U;
static uint32_t link_stats_last_tick_ms = 0U;

bool LinkStats_Init(void)
{
    memset(link_stats_slots, 0, sizeof(link_stats_slots));
    link_stats_index = 0U;
    link_stats_filled = 0U;
    link_stats_total = 0U;
    link_stats_last_tick_ms = 0U;
    printf("[LINK] INIT OK\r\n");
    return true;
}

void LinkStats_Process(uint32_t now_ms)
{
    if (((int32_t)(now_ms - link_stats_last_tick_ms)) <
        (int32_t)LINK_STATS_SLOT_MS)
    {
        return;
    }
    link_stats_last_tick_ms = now_ms;
    link_stats_slots[link_stats_index] = 0U; /* 新时隙默认未收到 */
    link_stats_index = (uint8_t)((link_stats_index + 1U) %
                                 GATEWAY_LINK_STATS_WINDOW);
    if (link_stats_filled < GATEWAY_LINK_STATS_WINDOW)
    {
        ++link_stats_filled;
    }
}

void LinkStats_NoteTelemetry(void)
{
    uint8_t index;

    /* 遥测属于刚结束的 20 s 周期 → 标记最新一个时隙 */
    index = (uint8_t)((link_stats_index + GATEWAY_LINK_STATS_WINDOW - 1U) %
                      GATEWAY_LINK_STATS_WINDOW);
    if (link_stats_filled > 0U)
    {
        link_stats_slots[index] = 1U;
    }
    if (link_stats_total < UINT16_MAX)
    {
        ++link_stats_total;
    }
}

uint16_t LinkStats_GetTotalTelemetry(void)
{
    return link_stats_total;
}

uint16_t LinkStats_GetWindowSlots(void)
{
    return (uint16_t)link_stats_filled;
}

uint8_t LinkStats_GetSuccessRatePercent(void)
{
    uint16_t success = 0U;
    uint8_t index;

    if (link_stats_filled == 0U)
    {
        return 0U;
    }
    for (index = 0U; index < link_stats_filled; ++index)
    {
        if (link_stats_slots[index] != 0U)
        {
            ++success;
        }
    }
    return (uint8_t)(((uint32_t)success * 100U) / link_stats_filled);
}
