# ESP32-S3-LCD-1.47 - Hardware Reference Documentation

Source: https://www.waveshare.com/wiki/ESP32-S3-LCD-1.47

---

## 1. Overview

Development board based on the ESP32-S3 with an integrated 1.47-inch TFT LCD display,
a TF card slot and an RGB LED. WiFi and Bluetooth 5 connectivity with an onboard ceramic antenna.

---

## 2. Processor and Memory Specifications

| Parameter        | Value                                     |
|------------------|-------------------------------------------|
| SoC              | ESP32-S3R8                                |
| Architecture     | Xtensa 32-bit LX7 dual-core              |
| Max frequency    | 240 MHz                                   |
| SRAM             | 512 KB                                    |
| ROM              | 384 KB                                    |
| PSRAM            | 8 MB                                      |
| Flash            | 16 MB                                     |
| USB              | Integrated full-speed USB serial          |

---

## 3. LCD Display

| Parameter        | Value                                     |
|------------------|-------------------------------------------|
| Size             | 1.47 inch TFT                             |
| Resolution       | 172 x 320 pixel                           |
| Colors           | 262K (18-bit)                             |
| Controller       | ST7789                                    |
| Interface        | SPI                                       |

---

## 4. Wireless Connectivity

| Parameter        | Value                                     |
|------------------|-------------------------------------------|
| WiFi             | 2.4 GHz, 802.11 b/g/n                    |
| Bluetooth        | Bluetooth 5 (BLE)                         |
| Antenna          | Onboard ceramic patch                     |

---

## 5. Power

| Parameter        | Value                                     |
|------------------|-------------------------------------------|
| LDO regulator    | ME6217C33M5G                              |
| Max current      | 800 mA                                    |
| Connector        | USB Type-A                                |

---

## 6. GPIO Pinout - Complete Mapping

### 6.1 LCD Display (SPI)

| Signal           | GPIO | Description                          |
|------------------|------|--------------------------------------|
| MOSI             | 45   | Master Out Slave In (SPI data)       |
| SCLK             | 40   | Serial Clock (SPI clock)             |
| CS               | 42   | Chip Select                          |
| DC               | 41   | Data/Command (data/cmd select)       |
| RST              | 39   | Display reset                        |
| BL               | 48   | Backlight                            |

### 6.2 TF Card (SD/MMC)

| Signal           | GPIO | Description                          |
|------------------|------|--------------------------------------|
| CMD              | 15   | Command line                         |
| SCK              | 14   | Clock                                |
| D0               | 16   | Data 0                               |
| D1               | 18   | Data 1                               |
| D2               | 17   | Data 2                               |
| D3               | 21   | Data 3                               |

### 6.3 Onboard Peripherals

| Component        | GPIO | Description                          |
|------------------|------|--------------------------------------|
| RGB LED          | 38   | Controllable RGB LED (WS2812-like)   |

---

## 7. Development Environment

### 7.1 Supported Frameworks

- **Arduino IDE** - requires the package `esp32 by Espressif Systems >= 3.0.2`
- **ESP-IDF** - via VSCode with the Espressif plugin

### 7.2 Required Libraries (offline installation)

| Library          | Version  | Use                                  |
|------------------|----------|--------------------------------------|
| LVGL             | 8.3.10   | Graphics library for UI              |
| PNGdec           | 1.0.2    | PNG image decoding                   |

### 7.3 Programming Tools

- Flash Download Tool v3.9.5_0
- OV2640/OV5640 camera support (optional)

---

## 8. Download/Flash Procedure

### Download Mode (if the flash fails):

1. Hold down the **BOOT** button
2. Simultaneously press **RESET**
3. Release **RESET**
4. Release **BOOT**
5. The device enters download mode

### USB Output:

- Enable `USB CDC On Boot` in the board settings
- Or declare `HWCDC` in the code for direct output via USB

---

## 9. Available Demos (reference)

| Demo                        | Description                                    |
|-----------------------------|------------------------------------------------|
| LVGL_Arduino                | Device functionality test with LVGL            |
| LCD_Image                   | Displaying PNG images from a TF card           |
| ESP32-S3-LCD-1.47-Test      | Full test of all peripherals                   |

---

## 10. Resources and Datasheets

- ESP32-S3 Technical Reference Manual (Espressif)
- LCD 1.47" Datasheet (ST7789)
- ESP32-S3-LCD-1.47 schematic (Waveshare)
- Demo code: ESP32-S3-LCD-1.47-Demo.zip

---

## 11. Notes for Custom Firmware

### SPI Configuration for the ST7789 LCD

```c
// Pin definitions for our firmware
#define LCD_MOSI    45
#define LCD_SCLK    40
#define LCD_CS      42
#define LCD_DC      41
#define LCD_RST     39
#define LCD_BL      48

// Display parameters
#define LCD_WIDTH   172
#define LCD_HEIGHT  320
#define LCD_COLOR_DEPTH  16  // RGB565 typically used
```

### SD Card Configuration

```c
// SD Card pin definitions (4-bit SD/MMC mode)
#define SD_CMD      15
#define SD_CLK      14
#define SD_D0       16
#define SD_D1       18
#define SD_D2       17
#define SD_D3       21
```

### RGB LED

```c
// NeoPixel / WS2812 RGB LED
#define RGB_LED_PIN 38
#define RGB_LED_NUM 1
```
