// Polled + confirmed Buxtronix software debounce — the proven decoder.
//
// Strategy:
//   1. A hardware timer fires every SAMPLE_US (5 kHz) and samples CLK/DT
//      from its ISR, so sampling never stalls when the loop is blocked in
//      an OLED redraw or Serial write.
//   2. A new pin reading must hold steady for CONFIRM_COUNT consecutive
//      samples before it is fed to the state machine — this rejects any
//      bounce shorter than ~SAMPLE_US * CONFIRM_COUNT.
//   3. Confirmed transitions drive a full-step Buxtronix state machine:
//      it only emits a step on a complete CW or CCW quadrature cycle.
//      Bounce that doesn't complete a cycle harmlessly returns to R_START.
//
// The ISR only touches the statics below — no Serial, no display, no State.
// The main loop reads pos/missed via the getters and bridges them into the
// shared counters from task context.

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

static hw_timer_t*    sampleTimer = nullptr;

// Touched by the ISR. pos/missed are read by the loop, so they are volatile;
// 32-bit aligned reads are atomic on this RISC-V core.
static volatile int32_t  pos        = 0;
static volatile uint32_t missed     = 0;
static uint8_t           encState   = R_START;
static uint8_t           lastStable = 0b11;
static uint8_t           candidate  = 0b11;
static uint8_t           confirm    = 0;

// digitalRead() is used in the ISR for readability; this sketch never writes
// flash at runtime, so the usual "ISR must avoid flash-backed calls" hazard
// doesn't arise. The whole routine is kept in IRAM regardless.
static void IRAM_ATTR onSample() {
  uint8_t pins = (digitalRead(CLK_PIN) << 1) | digitalRead(DT_PIN);

  if (pins == candidate) {
    if (confirm < 255) confirm++;
  } else {
    candidate = pins;
    confirm   = 1;
  }

  if (confirm >= CONFIRM_COUNT && candidate != lastStable) {
    // Valid quadrature changes exactly one bit at a time. If both flipped
    // between confirmed samples, an intermediate phase was too brief to
    // catch — we skipped (at least) one step.
    if ((candidate ^ lastStable) == 0b11) missed++;

    lastStable = candidate;
    encState   = TTABLE[encState & 0x0F][candidate];
    uint8_t dir = encState & 0xF0;
    if      (dir == DIR_CW)  pos++;
    else if (dir == DIR_CCW) pos--;
  }
}

void encoder_init() {
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN,  INPUT_PULLUP);
  pos        = 0;
  missed     = 0;
  encState   = R_START;
  lastStable = (digitalRead(CLK_PIN) << 1) | digitalRead(DT_PIN);
  candidate  = lastStable;
  confirm    = 0;

  // 1 MHz timer base (1 tick = 1 us), fire every SAMPLE_US ticks, auto-reload.
  sampleTimer = timerBegin(1000000);
  timerAttachInterrupt(sampleTimer, &onSample);
  timerAlarm(sampleTimer, SAMPLE_US, true, 0);
}

void encoder_reset() {
  // Stop the timer so the ISR can't run while we clear shared state.
  if (sampleTimer) timerStop(sampleTimer);
  pos        = 0;
  missed     = 0;
  encState   = R_START;
  lastStable = (digitalRead(CLK_PIN) << 1) | digitalRead(DT_PIN);
  candidate  = lastStable;
  confirm    = 0;
  if (sampleTimer) timerStart(sampleTimer);
}

int32_t  encoder_getPos()    { return pos; }
uint32_t encoder_getMissed() { return missed; }
