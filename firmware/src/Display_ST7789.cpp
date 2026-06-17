/*****************************************************************************
 *  Display_ST7789.cpp — LilyGo T-Display-S3 port
 *
 *  Upstream drove the Waveshare ST7789 over a hand-rolled SPI transport. The
 *  T-Display-S3 drives the same ST7789 controller over an 8-bit PARALLEL (i80)
 *  bus, so the transport is delegated to TFT_eSPI (configured via build_flags,
 *  TFT_eSPI Setup206). The public API (LCD_Init / LCD_addWindow / backlight) is
 *  unchanged, so LVGL_Driver.cpp and the rest of the firmware are untouched.
 ******************************************************************************/
#include "Display_ST7789.h"
#include <TFT_eSPI.h>

static TFT_eSPI tft = TFT_eSPI();

void LCD_Init(void)
{
  // Power-enable rail: required on the T-Display-S3 or the panel stays dark.
  pinMode(EXAMPLE_PIN_NUM_PWR_EN, OUTPUT);
  digitalWrite(EXAMPLE_PIN_NUM_PWR_EN, HIGH);

  Backlight_Init();

  tft.init();
  tft.setRotation(1);            // landscape 320(w) x 170(h); use 3 to flip 180°
  // LVGL stores 16-bit colour little-endian (LV_COLOR_16_SWAP=0); TFT_eSPI pushes
  // big-endian, so let it byte-swap our buffers. Flip this if colours look wrong.
  tft.setSwapBytes(true);
  tft.fillScreen(TFT_BLACK);
}

// Push an LVGL-rendered rectangle to the panel. Coordinates are inclusive.
void LCD_addWindow(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t* color)
{
  uint32_t w = (uint32_t)(Xend - Xstart + 1);
  uint32_t h = (uint32_t)(Yend - Ystart + 1);
  tft.startWrite();
  tft.setAddrWindow(Xstart, Ystart, w, h);
  tft.pushPixels(color, w * h);
  tft.endWrite();
}

// ----- Backlight (ledc PWM on GPIO38) -----
// Supports both arduino-esp32 2.0.x (ledcSetup+ledcAttachPin) and 3.x (ledcAttach).
#define BL_LEDC_CHANNEL 0

void Backlight_Init(void)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(EXAMPLE_PIN_NUM_BK_LIGHT, Frequency, Resolution);
  ledcWrite(EXAMPLE_PIN_NUM_BK_LIGHT, 1000);
#else
  ledcSetup(BL_LEDC_CHANNEL, Frequency, Resolution);
  ledcAttachPin(EXAMPLE_PIN_NUM_BK_LIGHT, BL_LEDC_CHANNEL);
  ledcWrite(BL_LEDC_CHANNEL, 1000);
#endif
}

void Set_Backlight(uint8_t Light)
{
  if (Light > 100) {
    printf("Set Backlight parameters in the range of 0 to 100 \r\n");
    return;
  }
  uint32_t Backlight = Light * 10;   // 0..100 -> 0..1000 (10-bit duty)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(EXAMPLE_PIN_NUM_BK_LIGHT, Backlight);
#else
  ledcWrite(BL_LEDC_CHANNEL, Backlight);
#endif
}
