#include "oled.h"
#include "i2c.h"
#include <string.h>

#define OLED_I2C_ADDRESS   0x78U
#define OLED_CONTROL_CMD   0x00U
#define OLED_CONTROL_DATA  0x40U
#define OLED_WIDTH         128U
#define OLED_PAGE_COUNT    8U

static uint8_t oled_frame_buffer[OLED_PAGE_COUNT][OLED_WIDTH];

static const uint8_t oled_temperature_icon[32] = {
    0x00U, 0x00U, 0x04U, 0x1CU, 0x1CU, 0x6CU, 0x00U, 0xFEU,
    0xF1U, 0xF1U, 0xFFU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x3CU, 0x66U, 0x83U,
    0xBFU, 0xBFU, 0x99U, 0x42U, 0x3CU, 0x00U, 0x00U, 0x00U
};

static HAL_StatusTypeDef OLED_Write(uint8_t control, uint8_t *data, uint16_t length)
{
    return HAL_I2C_Mem_Write(&hi2c1, OLED_I2C_ADDRESS, control,
                             I2C_MEMADD_SIZE_8BIT, data, length, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef OLED_WriteCommand(uint8_t command)
{
    return OLED_Write(OLED_CONTROL_CMD, &command, 1U);
}

static HAL_StatusTypeDef OLED_SetPosition(uint8_t x, uint8_t page)
{
    HAL_StatusTypeDef status;

    status = OLED_WriteCommand((uint8_t)(0xB0U + page));
    if (status != HAL_OK)
    {
        return status;
    }
    status = OLED_WriteCommand((uint8_t)(x & 0x0FU));
    if (status != HAL_OK)
    {
        return status;
    }
    return OLED_WriteCommand((uint8_t)(((x & 0xF0U) >> 4U) | 0x10U));
}

static const uint8_t *OLED_GetGlyph(char character)
{
    static const uint8_t blank[5] = {0U, 0U, 0U, 0U, 0U};
    static const uint8_t glyph_i[5] = {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U};
    static const uint8_t glyph_c[5] = {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U};
    static const uint8_t glyph_o[5] = {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU};
    static const uint8_t glyph_k[5] = {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U};
    static const uint8_t glyph_n[5] = {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU};
    static const uint8_t glyph_u[5] = {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU};
    static const uint8_t glyph_m[5] = {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU};
    static const uint8_t glyph_1[5] = {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U};
    static const uint8_t glyph_2[5] = {0x42U, 0x61U, 0x51U, 0x49U, 0x46U};
    static const uint8_t glyph_3[5] = {0x22U, 0x41U, 0x49U, 0x49U, 0x36U};

    switch (character)
    {
        case 'I': return glyph_i;
        case 'C': return glyph_c;
        case 'O': return glyph_o;
        case 'K': return glyph_k;
        case 'N': return glyph_n;
        case 'U': return glyph_u;
        case 'M': return glyph_m;
        case '1': return glyph_1;
        case '2': return glyph_2;
        case '3': return glyph_3;
        default: return blank;
    }
}

HAL_StatusTypeDef OLED_Init(void)
{
    static const uint8_t init_commands[] = {
        0xAEU, 0x00U, 0x10U, 0x40U, 0xB0U, 0x81U, 0xFFU, 0xA1U,
        0xA6U, 0xA8U, 0x3FU, 0xC8U, 0xD3U, 0x00U, 0xD5U, 0x80U,
        0xD8U, 0x05U, 0xD9U, 0xF1U, 0xDAU, 0x12U, 0xDBU, 0x30U,
        0x8DU, 0x14U, 0xAFU
    };
    uint32_t index;
    HAL_StatusTypeDef status;

    HAL_Delay(200U);
    for (index = 0U; index < (sizeof(init_commands) / sizeof(init_commands[0])); ++index)
    {
        status = OLED_WriteCommand(init_commands[index]);
        if (status != HAL_OK)
        {
            return status;
        }
    }
    return HAL_OK;
}

HAL_StatusTypeDef OLED_TestPattern(void)
{
    uint8_t page;
    uint8_t column;
    uint8_t pattern[OLED_WIDTH];
    HAL_StatusTypeDef status;

    for (page = 0U; page < OLED_PAGE_COUNT; ++page)
    {
        status = OLED_SetPosition(0U, page);
        if (status != HAL_OK)
        {
            return status;
        }
        for (column = 0U; column < OLED_WIDTH; ++column)
        {
            pattern[column] = (((column >> 3U) + page) & 1U) ? 0x55U : 0xAAU;
        }
        status = OLED_Write(OLED_CONTROL_DATA, pattern, OLED_WIDTH);
        if (status != HAL_OK)
        {
            return status;
        }
    }
    return HAL_OK;
}

HAL_StatusTypeDef OLED_Clear(void)
{
    uint8_t page;
    uint8_t blank[OLED_WIDTH] = {0U};
    HAL_StatusTypeDef status;

    memset(oled_frame_buffer, 0, sizeof(oled_frame_buffer));
    for (page = 0U; page < OLED_PAGE_COUNT; ++page)
    {
        status = OLED_SetPosition(0U, page);
        if (status != HAL_OK)
        {
            return status;
        }
        status = OLED_Write(OLED_CONTROL_DATA, blank, OLED_WIDTH);
        if (status != HAL_OK)
        {
            return status;
        }
    }
    return HAL_OK;
}

void OLED_FrameClear(void)
{
    memset(oled_frame_buffer, 0, sizeof(oled_frame_buffer));
}

void OLED_DrawPixel(uint8_t x, uint8_t y, uint8_t color)
{
    uint8_t mask;

    if ((x >= OLED_WIDTH) || (y >= (OLED_PAGE_COUNT * 8U)))
    {
        return;
    }

    mask = (uint8_t)(1U << (y & 0x07U));
    if (color != 0U)
    {
        oled_frame_buffer[y >> 3U][x] |= mask;
    }
    else
    {
        oled_frame_buffer[y >> 3U][x] &= (uint8_t)(~mask);
    }
}

void OLED_FillRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                   uint8_t color)
{
    uint16_t row;
    uint16_t column;

    for (row = y; (row < ((uint16_t)y + height)) &&
                  (row < (OLED_PAGE_COUNT * 8U)); ++row)
    {
        for (column = x; (column < ((uint16_t)x + width)) &&
                         (column < OLED_WIDTH); ++column)
        {
            OLED_DrawPixel((uint8_t)column, (uint8_t)row, color);
        }
    }
}

void OLED_DrawBitmap(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                     const uint8_t *bitmap)
{
    uint8_t bytes_per_row;
    uint16_t row;
    uint16_t column;
    uint16_t byte_index;

    if ((bitmap == NULL) || (width == 0U) || (height == 0U))
    {
        return;
    }

    bytes_per_row = (uint8_t)((width + 7U) / 8U);
    for (row = 0U; row < height; ++row)
    {
        for (column = 0U; column < width; ++column)
        {
            if ((((uint16_t)x + column) >= OLED_WIDTH) ||
                (((uint16_t)y + row) >= (OLED_PAGE_COUNT * 8U)))
            {
                continue;
            }
            byte_index = (uint16_t)(row * bytes_per_row) + (column >> 3U);
            if ((bitmap[byte_index] & (uint8_t)(0x80U >> (column & 0x07U))) != 0U)
            {
                OLED_DrawPixel((uint8_t)(x + column), (uint8_t)(y + row), 1U);
            }
        }
    }
}

HAL_StatusTypeDef OLED_Refresh(void)
{
    uint8_t page;
    HAL_StatusTypeDef status;

    for (page = 0U; page < OLED_PAGE_COUNT; ++page)
    {
        status = OLED_SetPosition(0U, page);
        if (status != HAL_OK)
        {
            return status;
        }
        status = OLED_Write(OLED_CONTROL_DATA, oled_frame_buffer[page], OLED_WIDTH);
        if (status != HAL_OK)
        {
            return status;
        }
    }
    return HAL_OK;
}

HAL_StatusTypeDef OLED_ShowText(uint8_t x, uint8_t page, const char *text)
{
    HAL_StatusTypeDef status;
    const uint8_t spacing = 0U;

    while ((*text != '\0') && (page < OLED_PAGE_COUNT) && ((x + 5U) < OLED_WIDTH))
    {
        status = OLED_SetPosition(x, page);
        if (status != HAL_OK)
        {
            return status;
        }
        status = OLED_Write(OLED_CONTROL_DATA, (uint8_t *)OLED_GetGlyph(*text), 5U);
        if (status != HAL_OK)
        {
            return status;
        }
        status = OLED_Write(OLED_CONTROL_DATA, (uint8_t *)&spacing, 1U);
        if (status != HAL_OK)
        {
            return status;
        }
        x = (uint8_t)(x + 6U);
        ++text;
    }
    return HAL_OK;
}

HAL_StatusTypeDef OLED_ShowTemperatureIcon(uint8_t x, uint8_t page)
{
    HAL_StatusTypeDef status;

    if (((x + 16U) > OLED_WIDTH) || ((page + 2U) > OLED_PAGE_COUNT))
    {
        return HAL_ERROR;
    }

    status = OLED_SetPosition(x, page);
    if (status != HAL_OK)
    {
        return status;
    }
    status = OLED_Write(OLED_CONTROL_DATA, (uint8_t *)oled_temperature_icon, 16U);
    if (status != HAL_OK)
    {
        return status;
    }
    status = OLED_SetPosition(x, (uint8_t)(page + 1U));
    if (status != HAL_OK)
    {
        return status;
    }
    return OLED_Write(OLED_CONTROL_DATA, (uint8_t *)&oled_temperature_icon[16], 16U);
}
