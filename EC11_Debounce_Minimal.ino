// EC11_Debounce_Minimal.ino — drop-in software-debounced EC11 reader
// =====================================================================
// A self-contained, library-free quadrature decoder for a mechanical EC11
// rotary encoder on the ESP32 / ESP32-C3 (Arduino-ESP32 core 3.x).
//
// Copy this whole file into your project and call:
//     encoderBegin();           // once, in setup()
//     long p = encoderPosition(); // anytime — signed detent count
// One unit of position = one detent. Turn it into "what changed since last
// time" by remembering the previous value (see loop() below).
//
// Why it's robust:
//   * A hardware timer samples CLK/DT every SAMPLE_US from an ISR, so the
//     decoder keeps counting even while your main loop is busy (drawing a
//     screen, talking to WiFi, blocking on I/O...).
//   * A new pin reading must repeat for CONFIRM_COUNT samples before it
//     counts, which rejects contact bounce shorter than that window.
//   * A full-step Buxtronix state machine only emits on a complete CW/CCW
//     quadrature cycle, so partial bounces never produce phantom steps.
//
// To adapt it:
//   1. Set the three pins below to match your wiring (encoder common -> GND).
//   2. Leave SAMPLE_US / CONFIRM_COUNT alone unless you have a reason; the
//      defaults handle a noisy hand-spun EC11 well.
//   3. That's it — the decoder doesn't need to know your detents-per-rev.
//
// Notes:
//   * This uses the Arduino-ESP32 *core 3.x* timer API (timerBegin(freq) +
//     timerAlarm). On core 2.x the API differs — see the comment in
//     encoderBegin().
//   * To open this standalone in the Arduino IDE, put it in a folder of the
//     same name: EC11_Debounce_Minimal/EC11_Debounce_Minimal.ino
// =====================================================================

// ── Wiring — change these to match your board ─────────────────────────
#define ENC_CLK_PIN   0      // encoder pin A (CLK)
#define ENC_DT_PIN    1      // encoder pin B (DT)
#define ENC_SW_PIN    3      // encoder push switch (optional, see loop())

// ── Debounce tuning ───────────────────────────────────────────────────
#define SAMPLE_US     200    // sample period in microseconds (200 = 5 kHz)
#define CONFIRM_COUNT 2      // identical samples required to accept a reading

// ── Full-step Buxtronix quadrature state machine ──────────────────────
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

static hw_timer_t*       encTimer   = nullptr;
static volatile int32_t  encPos     = 0;        // read by encoderPosition()
static uint8_t           encState   = R_START;
static uint8_t           lastStable = 0b11;
static uint8_t           candidate  = 0b11;
static uint8_t           confirm    = 0;

// Runs every SAMPLE_US in interrupt context — keep it short, no Serial here.
static void IRAM_ATTR encSampleISR() {
  uint8_t pins = (digitalRead(ENC_CLK_PIN) << 1) | digitalRead(ENC_DT_PIN);

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
    if      (dir == DIR_CW)  encPos++;
    else if (dir == DIR_CCW) encPos--;
  }
}

// Call once in setup().
void encoderBegin() {
  pinMode(ENC_CLK_PIN, INPUT_PULLUP);
  pinMode(ENC_DT_PIN,  INPUT_PULLUP);
  encPos     = 0;
  encState   = R_START;
  lastStable = (digitalRead(ENC_CLK_PIN) << 1) | digitalRead(ENC_DT_PIN);
  candidate  = lastStable;
  confirm    = 0;

  // Core 3.x API: 1 MHz base (1 tick = 1 us), fire every SAMPLE_US, auto-reload.
  encTimer = timerBegin(1000000);
  timerAttachInterrupt(encTimer, &encSampleISR);
  timerAlarm(encTimer, SAMPLE_US, true, 0);

  // --- Arduino-ESP32 core 2.x equivalent (replace the three lines above) ---
  // encTimer = timerBegin(0, 80, true);            // 80 MHz / 80 = 1 MHz
  // timerAttachInterrupt(encTimer, &encSampleISR, true);
  // timerAlarmWrite(encTimer, SAMPLE_US, true);
  // timerAlarmEnable(encTimer);
}

// Signed detent count since encoderBegin(). 32-bit read is atomic here.
int32_t encoderPosition() { return encPos; }

// =====================================================================
// Example usage — delete everything below when integrating into your code.
// =====================================================================

static int32_t       lastPos      = 0;
static int           lastSwState  = HIGH;   // INPUT_PULLUP: released = HIGH
static unsigned long lastSwChange = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(ENC_SW_PIN, INPUT_PULLUP);   // optional switch
  encoderBegin();
  lastPos = encoderPosition();
  Serial.println("EC11 ready — turn or press.");
}

void loop() {
  // Rotation: act on each detent of change since last time.
  int32_t pos = encoderPosition();
  while (pos != lastPos) {
    bool cw = (pos > lastPos);
    lastPos += cw ? 1 : -1;
    Serial.println(cw ? "CW" : "CCW");
  }

  // Optional push switch — simple time-window debounce on the falling edge.
  int sw = digitalRead(ENC_SW_PIN);
  if (sw != lastSwState && (millis() - lastSwChange) > 30) {
    lastSwChange = millis();
    lastSwState  = sw;
    if (sw == LOW) Serial.println("CLICK");
  }
}
