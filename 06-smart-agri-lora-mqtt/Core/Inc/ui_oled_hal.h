#ifndef UI_OLED_HAL_H
#define UI_OLED_HAL_H

#include <stdbool.h>
#include <stdint.h>

/*
 * OLED/按键硬件抽象：ui_oled 只依赖本接口。
 * 固件实现见 ui_hal.c（映射到 oled.c 16x16 点阵字体与 GPIO），主机单测用桩。
 * 注意：UiHal_DrawText 使用像素坐标 (x, y)，支持 UTF-8 中文字符（字库限制见
 * FontDotMatrix16）；每行最多 8 个 16px 字形。
 */

void UiHal_Init(void);
void UiHal_Clear(void); /* 仅清显存，不写屏幕（避免闪烁） */
void UiHal_DrawText(uint8_t x, uint8_t y, const char *text);
void UiHal_Refresh(void); /* 整帧推送一次 */
void UiHal_SetPower(bool on);
uint8_t UiHal_ReadKey1(void); /* 1=SW1 按下（低电平） */
uint8_t UiHal_ReadKey2(void); /* 1=SW2 按下（低电平） */

#endif /* UI_OLED_HAL_H */
