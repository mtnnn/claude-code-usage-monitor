#pragma once
#include "Arduino.h"

// The ESP32 core already defines PIN_NEOPIXEL=48 for the DevKitM-1; on the
// Waveshare LCD-1.47 the onboard RGB LED is instead on GPIO 38.
#define RGB_LED_PIN 38

void Set_Color(uint8_t Red,uint8_t Green,uint8_t Blue);                 // Set RGB bead color
void RGB_Lamp_Loop(uint16_t Waiting);                                   // The lamp beads change color in cycles