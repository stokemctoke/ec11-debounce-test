// Software-debounced EC11 decoder — the one method this sketch teaches.
//
// Strategy (see Encoder.cpp for the full explanation):
//   poll CLK/DT every SAMPLE_US, require CONFIRM_COUNT identical samples
//   before accepting a transition, then feed it to a full-step Buxtronix
//   quadrature state machine that only emits on a complete detent.
//
//   encoder_init()   — claim pins, zero the counter
//   encoder_poll()   — call every loop iteration
//   encoder_getPos() — signed detent count since init (1 unit = 1 detent)
#pragma once
#include <Arduino.h>

extern const char* ENCODER_NAME;

void    encoder_init();
void    encoder_poll();
int32_t encoder_getPos();
