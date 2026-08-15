/* link_stats 主机单元测试（任务书 T01 口径：20 s 时隙）。 */
#include "link_stats.h"

#include <stdio.h>

#define CHECK(condition)                                                        \
    do                                                                          \
    {                                                                           \
        if (!(condition))                                                       \
        {                                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);        \
            return 1;                                                           \
        }                                                                       \
    } while (0)

static int test_empty_and_full_window(void)
{
    unsigned int i;

    CHECK(LinkStats_Init() == true);
    CHECK(LinkStats_GetSuccessRatePercent() == 0U);
    CHECK(LinkStats_GetWindowSlots() == 0U);

    for (i = 1U; i <= GATEWAY_LINK_STATS_WINDOW; ++i)
    {
        LinkStats_Process(20000UL * i); /* 每个 20 s 周期推进时隙 */
        LinkStats_NoteTelemetry();      /* 每周期都收到遥测 */
    }
    CHECK(LinkStats_GetSuccessRatePercent() == 100U);
    CHECK(LinkStats_GetWindowSlots() == GATEWAY_LINK_STATS_WINDOW);
    CHECK(LinkStats_GetTotalTelemetry() == GATEWAY_LINK_STATS_WINDOW);
    return 0;
}

static int test_half_missed(void)
{
    unsigned int i;

    CHECK(LinkStats_Init() == true);
    for (i = 1U; i <= GATEWAY_LINK_STATS_WINDOW; ++i)
    {
        LinkStats_Process(20000UL * i);
        if ((i % 2U) == 0U) /* 隔一个周期收到 */
        {
            LinkStats_NoteTelemetry();
        }
    }
    CHECK(LinkStats_GetSuccessRatePercent() == 50U);
    return 0;
}

static int test_noise_does_not_count(void)
{
    unsigned int i;

    CHECK(LinkStats_Init() == true);
    for (i = 1U; i <= 10U; ++i)
    {
        LinkStats_Process(20000UL * i);
    }
    /* 无任何遥测 → 0%（噪声/其他帧不影响本统计） */
    CHECK(LinkStats_GetSuccessRatePercent() == 0U);
    CHECK(LinkStats_GetWindowSlots() == 10U);
    return 0;
}

static int test_sliding_window(void)
{
    unsigned int i;

    CHECK(LinkStats_Init() == true);
    for (i = 1U; i <= GATEWAY_LINK_STATS_WINDOW; ++i)
    {
        LinkStats_Process(20000UL * i);
        LinkStats_NoteTelemetry();
    }
    CHECK(LinkStats_GetSuccessRatePercent() == 100U);
    /* 之后 10 个周期全部丢帧 → 窗口内 40/50 = 80% */
    for (i = 1U; i <= 10U; ++i)
    {
        LinkStats_Process(20000UL * (GATEWAY_LINK_STATS_WINDOW + i));
    }
    CHECK(LinkStats_GetSuccessRatePercent() ==
          (uint8_t)(((GATEWAY_LINK_STATS_WINDOW - 10U) * 100U) /
                    GATEWAY_LINK_STATS_WINDOW));
    CHECK(LinkStats_GetWindowSlots() == GATEWAY_LINK_STATS_WINDOW);
    return 0;
}

static int test_process_throttles_within_slot(void)
{
    CHECK(LinkStats_Init() == true);
    LinkStats_Process(1000U); /* 距 0 不足 20 s：不推进 */
    CHECK(LinkStats_GetWindowSlots() == 0U);
    LinkStats_Process(15000U); /* 仍未到：不推进 */
    CHECK(LinkStats_GetWindowSlots() == 0U);
    LinkStats_Process(20000U); /* 到点：推进第 1 槽 */
    CHECK(LinkStats_GetWindowSlots() == 1U);
    LinkStats_Process(21000U); /* 仅过 1 s：不推进 */
    CHECK(LinkStats_GetWindowSlots() == 1U);
    LinkStats_Process(40000U); /* 推进第 2 槽 */
    CHECK(LinkStats_GetWindowSlots() == 2U);
    LinkStats_NoteTelemetry(); /* 标记刚结束的周期 */
    CHECK(LinkStats_GetSuccessRatePercent() == 50U);
    return 0;
}

int main(void)
{
    CHECK(test_empty_and_full_window() == 0);
    CHECK(test_half_missed() == 0);
    CHECK(test_noise_does_not_count() == 0);
    CHECK(test_sliding_window() == 0);
    CHECK(test_process_throttles_within_slot() == 0);
    puts("PASS link_stats");
    return 0;
}
