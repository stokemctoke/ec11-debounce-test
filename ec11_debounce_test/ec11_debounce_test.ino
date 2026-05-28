// Stage 04 (Super-Mini) — EC11 with Buxtronix debounce + click
// Target  : Tenstar Robot ESP32-C3 Super-Mini
// Encoder : CLK → GPIO0, DT → GPIO1, SW → GPIO3, Common → GND
// External WS2812 : data on GPIO10, 5V, GND
// Onboard LED : GPIO8 (single BLUE LED on Super-Mini, active LOW)
//
// Right turn → external RED,   onboard blinks
// Left  turn → external BLUE,  onboard blinks
// Click      → external GREEN, onboard blinks
//
// NOTE: Arduino IDE → Tools → "USB CDC On Boot: Enabled" so Serial reaches the monitor.

#include <FastLED.h>

// ── LED config ────────────────────────────────────────────────────────────────
#define EXT_PIN       10
#define ONBOARD_PIN    8       // single blue LED on Super-Mini, active LOW
#define ONBOARD_ON   LOW
#define ONBOARD_OFF  HIGH
#define NUM_LEDS       1
#define BRIGHTNESS    75
#define FLASH_MS     200

// ── Encoder pins ──────────────────────────────────────────────────────────────
#define CLK_PIN        0
#define DT_PIN         1
#define SW_PIN         3
#define SW_DEBOUNCE_MS 30

// ── Buxtronix full-step state machine ─────────────────────────────────────────
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

// ── Shared encoder state ──────────────────────────────────────────────────────
int32_t encoderPos = 0;
uint8_t encState   = R_START;

// ── Runtime state ─────────────────────────────────────────────────────────────
CRGB          extLeds[NUM_LEDS];
CRGB          flashColor = CRGB::Black;
unsigned long flashUntil = 0;
int32_t       lastPos    = 0;
uint32_t      stepNumber = 0;       // unified counter for every event
int            lastSwState = HIGH;  // EC11 button is open at idle → pulled high
unsigned long  lastSwChange = 0;

// ── Polled encoder sampler with stability filter ──────────────────────────────
// Sampled every SAMPLE_US microseconds. A new pin reading must be confirmed
// (same value seen on the next sample) before it's fed to the state machine.
// This rejects bounce shorter than ~SAMPLE_US.
#define SAMPLE_US     200    // 5 kHz sample rate
#define CONFIRM_COUNT  2     // must read the same value this many times

void sampleEncoder() {
  static unsigned long nextSample = 0;
  static uint8_t       lastStable = 0b11;
  static uint8_t       candidate  = 0b11;
  static uint8_t       confirm    = 0;

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
    if      (dir == DIR_CW)  encoderPos++;
    else if (dir == DIR_CCW) encoderPos--;
  }
}

// ── Helpers ───────────────────────────────────────────────────────────────────
void setExt(CRGB color)      { extLeds[0] = color; }
void setOnboard(bool on)     { digitalWrite(ONBOARD_PIN, on ? ONBOARD_ON : ONBOARD_OFF); }
void flash(CRGB color)       { flashColor = color; flashUntil = millis() + FLASH_MS; }

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);   // give USB-CDC time to reconnect after upload/reset

  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN,  INPUT_PULLUP);
  pinMode(SW_PIN,  INPUT_PULLUP);
  pinMode(ONBOARD_PIN, OUTPUT);
  setOnboard(false);

  FastLED.addLeds<WS2812, EXT_PIN, GRB>(extLeds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  // Startup: double-blink both LEDs together to confirm they're alive
  for (int i = 0; i < 2; i++) {
    setExt(CRGB::White); FastLED.show(); setOnboard(true);
    delay(250);
    setExt(CRGB::Black); FastLED.show(); setOnboard(false);
    delay(200);
  }

  Serial.println("================================");
  Serial.println(" EC11 Test — Super-Mini build");
  Serial.println(" CLK: GPIO0   DT: GPIO1   SW: GPIO3");
  Serial.println(" External LED: GPIO10");
  Serial.println(" Onboard LED:  GPIO8 (mirror)");
  Serial.println("================================");
  Serial.print  ("Idle pin state (should be 3): ");
  Serial.println((digitalRead(CLK_PIN) << 1) | digitalRead(DT_PIN));
  Serial.println("Rotate or press encoder...");
  Serial.println();
}

// Print "STEP NN" with zero-padded 2-digit number (3+ digits print naturally)
void printStep(uint32_t n) {
  if (n < 10) Serial.print('0');
  Serial.print(n);
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
  sampleEncoder();

  // ── Rotation events (one print per detent) ─────────────────────────────────
  int32_t pos = encoderPos;
  while (pos != lastPos) {
    bool cw = (pos > lastPos);
    lastPos += cw ? 1 : -1;
    stepNumber++;

    if (cw) {
      flash(CRGB::Red);
      Serial.print("--> RIGHT - STEP ");
    } else {
      flash(CRGB::Blue);
      Serial.print("<-- LEFT  - STEP ");
    }
    printStep(stepNumber);
    Serial.println();
  }

  // ── Button (press = falling edge, debounced) ───────────────────────────────
  int swReading = digitalRead(SW_PIN);
  if (swReading != lastSwState && (millis() - lastSwChange) > SW_DEBOUNCE_MS) {
    lastSwChange = millis();
    if (swReading == LOW) {  // pressed
      stepNumber++;
      flash(CRGB::Green);
      Serial.print("(*) CLICK - STEP ");
      printStep(stepNumber);
      Serial.println();
    }
    lastSwState = swReading;
  }

  bool active = (millis() < flashUntil);
  CRGB target = active ? flashColor : CRGB::Black;
  if (target != extLeds[0]) {
    setExt(target);
    FastLED.show();
    setOnboard(active);
  }
}
