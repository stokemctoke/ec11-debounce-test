// Pin assignments and timing constants — change here, not in the .cpp files.
#pragma once
#include <Arduino.h>

// ── External LED (WS2812) ─────────────────────────────────────────────────────
#define EXT_PIN       10
#define NUM_LEDS       1
#define BRIGHTNESS    75
#define FLASH_MS      75

// ── Onboard LED (Super-Mini blue LED, active LOW) ─────────────────────────────
#define ONBOARD_PIN    8
#define ONBOARD_ON   LOW
#define ONBOARD_OFF  HIGH

// ── EC11 encoder ──────────────────────────────────────────────────────────────
#define CLK_PIN        0
#define DT_PIN         1
#define SW_PIN         3
#define SW_DEBOUNCE_MS 30

// ── Encoder sampling (polled methods only) ────────────────────────────────────
#define SAMPLE_US     200    // 5 kHz sample rate
#define CONFIRM_COUNT  2     // confirm samples that must agree (polled+confirm)

// ── I2C / OLED ────────────────────────────────────────────────────────────────
#define SDA_PIN        6
#define SCL_PIN        7
