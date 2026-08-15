#include "ui_oled_hal.h"

#include "main.h"
#include "oled.h"

#include <stdio.h>

/*
 * ui_oled_hal 固件实现：映射到 SSD1306 驱动（16x16 点阵字体，支持字库内中文）
 * 与网关按键 GPIO。
 * 防闪烁约定：UiHal_Clear 只清显存；UiHal_Refresh 才推整帧。
 * OLED 缺失/故障时全部显示操作安全降级为无操作，绝不阻塞系统。
 */

static uint8_t ui_hal_oled_ok = 0U;

void UiHal_Init(void)
{
    ui_hal_oled_ok = (OLED_Init() == HAL_OK) ? 1U : 0U;
    if (ui_hal_oled_ok != 0U)
    {
        OLED_FrameClear();
        (void)OLED_Refresh();
    }
    else
    {
        printf("[UI] OLED FAIL\r\n");
    }
}

void UiHal_Clear(void)
{
    if (ui_hal_oled_ok != 0U)
    {
        OLED_FrameClear(); /* 只清显存，屏幕保持到下次 Refresh */
    }
}

void UiHal_DrawText(uint8_t x, uint8_t y, const char *text)
{
    if (ui_hal_oled_ok != 0U)
    {
        OLED_DrawUtf8Text16(x, y, text);
    }
}

void UiHal_Refresh(void)
{
    if (ui_hal_oled_ok != 0U)
    {
        (void)OLED_Refresh();
    }
}

void UiHal_SetPower(bool on)
{
    if (ui_hal_oled_ok == 0U)
    {
        return;
    }
    if (!on)
    {
        OLED_FrameClear();
        (void)OLED_Refresh(); /* 息屏 = 推一帧空白 */
    }
}

uint8_t UiHal_ReadKey1(void)
{
    return (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
               ? 1U
               : 0U;
}

uint8_t UiHal_ReadKey2(void)
{
    return (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET)
               ? 1U
               : 0U;
}
