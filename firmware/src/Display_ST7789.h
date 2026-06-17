#pragma once
#include <Arduino.h>

// LilyGo T-Display-S3: ST7789 170x320 over 8-bit parallel bus.
// The low-level transport (parallel bus, init sequence, address window, the
// 35px column offset, colour inversion/order) is handled by TFT_eSPI, configured
// via build_flags in platformio.ini (TFT_eSPI Setup206). This header only exposes
// the small API the rest of the firmware depends on.
// Landscape orientation: logical canvas is 320 wide x 170 tall. The physical
// panel is 170x320 (TFT_WIDTH/HEIGHT in platformio.ini); tft.setRotation(1) in
// LCD_Init() rotates it and TFT_eSPI applies the column offset for us.
#define LCD_WIDTH   320 // LCD width  (landscape)
#define LCD_HEIGHT  170 // LCD height (landscape)

// Backlight: GPIO38 on the T-Display-S3 (PWM via ledc).
#define EXAMPLE_PIN_NUM_BK_LIGHT       38
#define Frequency       1000                    // PWM frequency
#define Resolution      10                      // PWM resolution (bits)

// T-Display-S3 must drive the LCD/PSRAM power rail enable HIGH or the screen
// stays dark (especially on battery).
#define EXAMPLE_PIN_NUM_PWR_EN         15

void LCD_Init(void);
void LCD_addWindow(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t* color);

void Backlight_Init(void);
void Set_Backlight(uint8_t Light);
