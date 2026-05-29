// Software-debounced EC11 decoder — the one method this sketch teaches.
//
// Sampling runs in a hardware-timer ISR at SAMPLE_US (5 kHz) so it never
// misses transitions while the main loop is busy redrawing the OLED or
// printing to Serial. The decode logic is the same polled + confirmed
// Buxtronix state machine; only the cadence source changed.
//
//   encoder_init()    — claim pins, zero counters, start the sampling timer
//   encoder_reset()    — zero position + missed count (timer keeps running)
//   encoder_getPos()    — signed detent count since init (1 unit = 1 detent)
//   encoder_getMissed() — suspected skipped detents (Gray-code violations)
#pragma once
#include <Arduino.h>

extern const char* ENCODER_NAME;

void     encoder_init();
void     encoder_reset();
int32_t  encoder_getPos();
uint32_t encoder_getMissed();
