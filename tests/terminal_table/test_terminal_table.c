/* terminal_table 主机单元测试。 */
#include "terminal_table.h"

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

static int test_add_and_online_window(void)
{
    uint16_t id = 0U;
    bool online = false;

    CHECK(TerminalTable_Init() == true);
    CHECK(TerminalTable_GetCount() == 0U);

    TerminalTable_NoteTelemetry(2U, 1000U);
    CHECK(TerminalTable_GetCount() == 1U);
    CHECK(TerminalTable_IsOnline(2U, 1000U) == true);
    CHECK(TerminalTable_IsOnline(2U, 30999U) == true);
    CHECK(TerminalTable_IsOnline(2U, 31000U) == false); /* 超过 30 s 离线 */
    CHECK(TerminalTable_IsOnline(3U, 1000U) == false);  /* 未注册 */

    TerminalTable_NoteTelemetry(2U, 31000U);
    CHECK(TerminalTable_IsOnline(2U, 31000U) == true); /* 更新后在线 */

    CHECK(TerminalTable_GetEntry(0U, &id, &online, 31000U) == true);
    CHECK(id == 2U);
    CHECK(online == true);
    CHECK(TerminalTable_GetEntry(1U, &id, &online, 31000U) == false);
    return 0;
}

static int test_multiple_terminals_and_cap(void)
{
    unsigned int i;
    uint16_t id = 0U;
    bool online = false;

    CHECK(TerminalTable_Init() == true);
    for (i = 0U; i < TERMINAL_TABLE_MAX_TERMINALS; ++i)
    {
        TerminalTable_NoteTelemetry((uint16_t)(2U + i), 5000U);
    }
    CHECK(TerminalTable_GetCount() == TERMINAL_TABLE_MAX_TERMINALS);
    TerminalTable_NoteTelemetry(99U, 5000U); /* 表满：丢弃 */
    CHECK(TerminalTable_GetCount() == TERMINAL_TABLE_MAX_TERMINALS);
    CHECK(TerminalTable_IsOnline(99U, 5000U) == false);

    CHECK(TerminalTable_GetEntry(TERMINAL_TABLE_MAX_TERMINALS - 1U, &id,
                                 &online, 5000U) == true);
    CHECK(id == (uint16_t)(2U + TERMINAL_TABLE_MAX_TERMINALS - 1U));
    CHECK(online == true);

    TerminalTable_NoteTelemetry(2U, 70000U); /* 更新已有条目不新增 */
    CHECK(TerminalTable_GetCount() == TERMINAL_TABLE_MAX_TERMINALS);
    CHECK(TerminalTable_IsOnline(2U, 70000U) == true);
    return 0;
}

int main(void)
{
    CHECK(test_add_and_online_window() == 0);
    CHECK(test_multiple_terminals_and_cap() == 0);
    puts("PASS terminal_table");
    return 0;
}
