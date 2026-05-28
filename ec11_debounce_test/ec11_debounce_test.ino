// EC11 debounce test — Tenstar Robot ESP32-C3 Super-Mini
//
// Encoder        : CLK → GPIO0, DT → GPIO1, SW → GPIO3, Common → GND
// External WS2812: data on GPIO10, 5V, GND
// Onboard LED    : GPIO8 (single BLUE LED on Super-Mini, active LOW)
// I2C OLED (opt.): SDA → GPIO6, SCL → GPIO7, VCC → 3.3V, GND → GND
//                  (SSD1306 128x64. Auto-detected at boot — sketch runs without one.)
//
// Right turn → external RED,   onboard blinks
// Left  turn → external BLUE,  onboard blinks
// Click      → external GREEN, onboard blinks
//
// Libraries: FastLED, U8g2
// Arduino IDE → Tools → "USB CDC On Boot: Enabled" so Serial reaches the monitor.

#include <FastLED.h>
#include <Wire.h>
#include <U8g2lib.h>

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

// ── I2C / OLED ────────────────────────────────────────────────────────────────
#define SDA_PIN        6
#define SCL_PIN        7
// VCOMH0 init variant fixes a column-0 ghost on some SSD1306 clones.
// If your panel still shows a noisy left column, swap to _NONAME_ or _ALT0_.
U8G2_SSD1306_128X64_VCOMH0_F_HW_I2C oled(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
bool oledPresent = false;

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
int            lastSwState = HIGH;  // EC11 button is open at idle → pulled high
unsigned long  lastSwChange = 0;

// Counters
uint8_t  cycleStep  = 0;   // 1..20, rotations only — wraps after 20 (clicks excluded)
uint32_t cwCount    = 0;   // total CW rotations
uint32_t ccwCount   = 0;   // total CCW rotations
uint32_t clickCount = 0;   // total clicks

// Display state
enum LastAction { ACT_NONE, ACT_RIGHT, ACT_LEFT, ACT_CLICK };
LastAction lastAction = ACT_NONE;
bool       displayDirty = false;

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

// ── OLED / I2C helpers ────────────────────────────────────────────────────────
// Scan all I2C addresses and print any devices found. Useful for diagnosing
// "no OLED output" — confirms whether the bus is electrically alive.
void scanI2C() {
  Serial.print("I2C scan on SDA=GPIO");
  Serial.print(SDA_PIN);
  Serial.print(" SCL=GPIO");
  Serial.print(SCL_PIN);
  Serial.println(":");
  uint8_t found = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("  device at 0x");
      if (addr < 0x10) Serial.print('0');
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) Serial.println("  (no devices responded)");
}

bool detectOled() {
  for (uint8_t addr : { 0x3C, 0x3D }) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) return true;
  }
  return false;
}

void drawSplash() {
  if (!oledPresent) return;
  oled.clearBuffer();
  oled.setFont(u8g2_font_helvB12_tr);
  const char* title = "EC11 Debounce";
  int tw = oled.getStrWidth(title);
  oled.drawStr((128 - tw) / 2, 32, title);
  oled.setFont(u8g2_font_6x10_tr);
  const char* sub = "Ready";
  int sw = oled.getStrWidth(sub);
  oled.drawStr((128 - sw) / 2, 52, sub);
  oled.sendBuffer();
}

void updateDisplay() {
  if (!oledPresent) return;
  oled.clearBuffer();

  // Header
  oled.setFont(u8g2_font_5x8_tr);
  oled.drawStr(2, 8, "EC11 DEBOUNCE");
  oled.drawHLine(2, 10, 124);

  // Cycle counter "NN / 20", centered
  oled.setFont(u8g2_font_helvB14_tr);
  char buf[32];
  snprintf(buf, sizeof(buf), "%u / 20", cycleStep);
  int w = oled.getStrWidth(buf);
  oled.drawStr((128 - w) / 2, 30, buf);

  // Last action, centered
  oled.setFont(u8g2_font_6x10_tr);
  const char* a = "";
  switch (lastAction) {
    case ACT_RIGHT: a = "--> RIGHT"; break;
    case ACT_LEFT:  a = "<-- LEFT";  break;
    case ACT_CLICK: a = "(*) CLICK"; break;
    default:        a = "";
  }
  int aw = oled.getStrWidth(a);
  oled.drawStr((128 - aw) / 2, 46, a);

  // Bottom stats: total rotations and total clicks
  oled.setFont(u8g2_font_5x8_tr);
  snprintf(buf, sizeof(buf), "STEPS:%lu  CLICKS:%lu",
           (unsigned long)(cwCount + ccwCount),
           (unsigned long)clickCount);
  int sw = oled.getStrWidth(buf);
  oled.drawStr((128 - sw) / 2, 62, buf);

  oled.sendBuffer();
}

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

  // OLED: probe I2C, splash if present
  Wire.begin(SDA_PIN, SCL_PIN);
  scanI2C();   // diagnostic: prints all I2C devices found
  oledPresent = detectOled();
  if (oledPresent) {
    oled.begin();
    oled.setBusClock(400000);
    drawSplash();
  }

  Serial.println("================================");
  Serial.println(" EC11 Test — Super-Mini build");
  Serial.println(" CLK: GPIO0   DT: GPIO1   SW: GPIO3");
  Serial.println(" External LED: GPIO10");
  Serial.println(" Onboard LED:  GPIO8 (mirror)");
  Serial.print  (" OLED: ");
  Serial.println(oledPresent ? "detected on I2C" : "not found");
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

    cycleStep = (cycleStep % 20) + 1;   // 1..20, wraps

    if (cw) {
      cwCount++;
      lastAction = ACT_RIGHT;
      flash(CRGB::Red);
      Serial.print("--> RIGHT - STEP ");
    } else {
      ccwCount++;
      lastAction = ACT_LEFT;
      flash(CRGB::Blue);
      Serial.print("<-- LEFT  - STEP ");
    }
    printStep(cycleStep);
    Serial.println();
    displayDirty = true;
  }

  // ── Button (press = falling edge, debounced) ───────────────────────────────
  int swReading = digitalRead(SW_PIN);
  if (swReading != lastSwState && (millis() - lastSwChange) > SW_DEBOUNCE_MS) {
    lastSwChange = millis();
    if (swReading == LOW) {  // pressed (clicks excluded from cycle counter)
      clickCount++;
      lastAction = ACT_CLICK;
      flash(CRGB::Green);
      Serial.print("(*) CLICK - CLK ");
      printStep(clickCount);
      Serial.println();
      displayDirty = true;
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

  if (displayDirty) {
    updateDisplay();
    displayDirty = false;
  }
}
