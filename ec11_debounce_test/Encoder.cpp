// Polled + confirmed Buxtronix software debounce — the proven decoder.
//
// Strategy:
//   1. Sample CLK and DT every SAMPLE_US microseconds (5 kHz).
//   2. A new pin reading must hold steady for CONFIRM_COUNT consecutive
//      samples before it is fed to the state machine — this rejects any
//      bounce shorter than ~SAMPLE_US * CONFIRM_COUNT.
//   3. Confirmed transitions drive a full-step Buxtronix state machine:
//      it only emits a step on a complete CW or CCW quadrature cycle.
//      Bounce that doesn't complete a cycle harmlessly returns to R_START.
//
// Tradeoff: very robust against bounce; adds ~SAMPLE_US of latency.
// This is what we landed on after early attempts failed on a noisy EC11.

#include "Encoder.h"
#include "Config.h"

const char* ENCODER_NAME = "Polled+Confirm Buxtronix";

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
  /* R_START    */ {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START         },
  /* R_CW_FINAL */ {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START|DIR_CW  },
  /* R_CW_BEGIN */ {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START         },
  /* R_CW_NEXT  */ {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START         },
  /* R_CCW_BEGIN*/ {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START         },
  /* R_CCW_FINAL*/ {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START|DIR_CCW },
  /* R_CCW_NEXT */ {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START         },
};

static int32_t       pos        = 0;
static uint8_t       encState   = R_START;
static unsigned long nextSample = 0;
static uint8_t       lastStable = 0b11;
static uint8_t       candidate  = 0b11;
static uint8_t       confirm    = 0;

void encoder_init() {
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN,  INPUT_PULLUP);
  pos        = 0;
  encState   = R_START;
  nextSample = 0;
  lastStable = (digitalRead(CLK_PIN) << 1) | digitalRead(DT_PIN);
  candidate  = lastStable;
  confirm    = 0;
}

int32_t encoder_getPos() { return pos; }

void encoder_poll() {
  unsigned long now = micros();
  if ((long)(now - nextSample) < 0) return;
  nextSample = now + SAMPLE_US;

  uint8_t pins = (digitalRead(CLK_PIN) << 1) | digitalRead(DT_PIN);

  if (pins == candidate) {
    if (confirm < 255) confirm++;
  } else {
    candidate = pins;
    confirm   = 1;
  }

  if (confirm >= CONFIRM_COUNT && candidate != lastStable) {
    lastStable = candidate;
    encState   = TTABLE[encState & 0x0F][candidate];
    uint8_t dir = encState & 0xF0;
    if      (dir == DIR_CW)  pos++;
    else if (dir == DIR_CCW) pos--;
  }
}
