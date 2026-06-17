#pragma once
#include <Arduino.h>

// BOOT button (GPIO 0) — polled, debounced.
//
// NB: GPIO0 is also the ESP32-S3 download-mode strap pin, so we do NOT
// use interrupts. Polling is OK because we call Button::poll() at high
// frequency from the main loop.
namespace Button {

enum Event {
  NONE,
  TAP,         // release within 500 ms
  LONG,        // release between 500 and 5000 ms
  VERY_LONG    // still pressed at 5000 ms (fires mid-press for feedback)
};

void begin();

// To be called on every iteration of the loop. Returns at most one event
// per call (consumed and cleared).
Event poll();

} // namespace Button
