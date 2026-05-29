// Encoder method: QUADRATURE EVENT MATRIX  (the textbook lookup decoder)
//
// A 16-entry table maps each (old_state, new_state) pair to:
//    +1 → valid CW quarter-step (one of the 4 transitions per detent)
//    -1 → valid CCW quarter-step
//     0 → no change
//    +2 → impossible transition (both pins flipped simultaneously)
//
// Sampled every SAMPLE_US, no stability filter. Each detent of a 20-step
// EC11 produces 4 valid transitions, so we divide the running quarter count
// by 4 when returning the detent position.
//
// Strengths: tiny code, easy to understand.
// Weaknesses: any bounce that produces a valid-looking transition counts as
// a real step — so spinning produces extra phantom counts unless the
// encoder is electrically very clean.

#include "Encoder.h"
#include "Config.h"

static const int8_t QEM[16] = {
   0, -1, +1,  2,
  +1,  0,  2, -1,
  -1,  2,  0, +1,
   2, +1, -1,  0
};

static int32_t quarterCounter = 0;
static uint8_t lastPins       = 0b11;

void qem_init() {
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN,  INPUT_PULLUP);
  quarterCounter = 0;
  lastPins = (digitalRead(CLK_PIN) << 1) | digitalRead(DT_PIN);
}

void qem_deinit() {
  // Nothing to release.
}

int32_t qem_getPos() {
  // 4 quarter-steps = 1 detent on a 20-step EC11.
  return quarterCounter / 4;
}

void qem_poll() {
  static unsigned long nextSample = 0;
  unsigned long now = micros();
  if ((long)(now - nextSample) < 0) return;
  nextSample = now + SAMPLE_US;

  uint8_t pins = (digitalRead(CLK_PIN) << 1) | digitalRead(DT_PIN);
  if (pins == lastPins) return;
  int8_t step = QEM[(lastPins << 2) | pins];
  lastPins = pins;
  if (step == +1 || step == -1) quarterCounter += step;
  // step == +2 is an "impossible" transition — ignore.
}
