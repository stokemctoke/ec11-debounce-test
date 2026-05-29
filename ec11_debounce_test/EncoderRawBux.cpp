// Encoder method: RAW ISR BUXTRONIX  (no software debounce — broken on purpose)
//
// Same Buxtronix state machine as the polled version, but driven by CHANGE
// interrupts on both pins instead of timed sampling. Every contact bounce
// fires the ISR, which walks the state machine through invalid transitions
// and resets to R_START — meaning very few complete cycles ever emit a step.
//
// On a clean encoder with hardware caps this would work; on a typical noisy
// EC11 the position barely moves no matter how fast you turn. Keep this
// method around as the "before" half of the debounce demo.

#include "Encoder.h"
#include "Config.h"

#define R_START     0x00
#define R_CW_FINAL  0x01
#define R_CW_BEGIN  0x02
#define R_CW_NEXT   0x03
#define R_CCW_BEGIN 0x04
#define R_CCW_FINAL 0x05
#define R_CCW_NEXT  0x06
#define DIR_CW      0x10
#define DIR_CCW     0x20

static const uint8_t TTABLE[7][4] = {
  {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START         },
  {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START|DIR_CW  },
  {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START         },
  {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START         },
  {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START         },
  {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START|DIR_CCW },
  {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START         },
};

static volatile int32_t pos      = 0;
static volatile uint8_t encState = R_START;

static void IRAM_ATTR encoderISR() {
  uint8_t pins = (digitalRead(CLK_PIN) << 1) | digitalRead(DT_PIN);
  encState = TTABLE[encState & 0x0F][pins];
  uint8_t dir = encState & 0xF0;
  if      (dir == DIR_CW)  pos++;
  else if (dir == DIR_CCW) pos--;
}

void rawBux_init() {
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN,  INPUT_PULLUP);
  pos      = 0;
  encState = R_START;
  attachInterrupt(digitalPinToInterrupt(CLK_PIN), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(DT_PIN),  encoderISR, CHANGE);
}

void rawBux_deinit() {
  detachInterrupt(digitalPinToInterrupt(CLK_PIN));
  detachInterrupt(digitalPinToInterrupt(DT_PIN));
}

int32_t rawBux_getPos() { return pos; }

void rawBux_poll() {
  // Nothing to do — ISRs drive everything.
}
