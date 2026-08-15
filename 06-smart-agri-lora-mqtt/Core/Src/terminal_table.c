#include "terminal_table.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    uint16_t id;
    uint32_t last_seen_ms;
} TerminalTableEntry;

static TerminalTableEntry terminal_table_entries[TERMINAL_TABLE_MAX_TERMINALS];
static uint8_t terminal_table_count = 0U;

bool TerminalTable_Init(void)
{
    memset(terminal_table_entries, 0, sizeof(terminal_table_entries));
    terminal_table_count = 0U;
    printf("[TABLE] INIT OK\r\n");
    return true;
}

void TerminalTable_NoteTelemetry(uint16_t terminal_id, uint32_t now_ms)
{
    uint8_t index;

    if (terminal_id == 0U)
    {
        return;
    }
    for (index = 0U; index < terminal_table_count; ++index)
    {
        if (terminal_table_entries[index].id == terminal_id)
        {
            terminal_table_entries[index].last_seen_ms = now_ms;
            return;
        }
    }
    if (terminal_table_count >= TERMINAL_TABLE_MAX_TERMINALS)
    {
        printf("[TABLE] FULL DROP ID=%u\r\n", (unsigned int)terminal_id);
        return;
    }
    terminal_table_entries[terminal_table_count].id = terminal_id;
    terminal_table_entries[terminal_table_count].last_seen_ms = now_ms;
    ++terminal_table_count;
    printf("[TABLE] ADD ID=%u\r\n", (unsigned int)terminal_id);
}

bool TerminalTable_IsOnline(uint16_t terminal_id, uint32_t now_ms)
{
    uint8_t index;

    for (index = 0U; index < terminal_table_count; ++index)
    {
        if (terminal_table_entries[index].id == terminal_id)
        {
            return ((int32_t)(now_ms -
                              terminal_table_entries[index].last_seen_ms)) <
                   (int32_t)GATEWAY_TERMINAL_OFFLINE_MS;
        }
    }
    return false;
}

uint8_t TerminalTable_GetCount(void)
{
    return terminal_table_count;
}

bool TerminalTable_GetEntry(uint8_t index,
                            uint16_t *terminal_id,
                            bool *online,
                            uint32_t now_ms)
{
    if ((index >= terminal_table_count) || (terminal_id == NULL) ||
        (online == NULL))
    {
        return false;
    }
    *terminal_id = terminal_table_entries[index].id;
    *online = TerminalTable_IsOnline(terminal_table_entries[index].id, now_ms);
    return true;
}
