// EC11 debounce test — Tenstar Robot ESP32-C3 Super-Mini
//
// Hardware (see Config.h for pin assignments):
//   Encoder         CLK → GPIO0, DT → GPIO1, SW → GPIO3, common → GND
//   External WS2812 data → GPIO10, 5V, GND
//   Onboard LED     GPIO8 (single blue, active LOW)
//   I2C OLED (opt.) SDA → GPIO6, SCL → GPIO7, VCC → 3.3V, GND → GND
//
// File map:
//   Config.h     — pin and timing constants
//   State.cpp    — shared counters + lastAction (recordRotation/Click/reset)
//   LedFx.cpp    — external WS2812 + onboard LED flash logic
//   Display.cpp  — SSD1306 OLED detection, splash, redraw
//   SerialOut.cpp — formatted Serial banner + event lines
//   Encoder.cpp  — polled + confirmed Buxtronix decoder, sampled in a timer ISR
//
// Encoder switch: short press = click event, hold for LONG_PRESS_MS = reset.
//
// Arduino IDE → Tools → "USB CDC On Boot: Enabled" so Serial reaches the monitor.
// Libraries  : FastLED, U8g2

#include "Config.h"
#include "State.h"
#include "LedFx.h"
#include "Display.h"
#include "SerialOut.h"
#include "Encoder.h"

static int32_t       lastPos      = 0;
static uint32_t      lastMissed   = 0;
static int           lastSwState  = HIGH;
static unsigned long lastSwChange = 0;
static unsigned long pressStart   = 0;
static bool          longFired    = false;

void setup() {
  Serial.begin(115200);
  delay(2000);   // give USB-CDC time to (re)connect after upload/reset

  pinMode(SW_PIN, INPUT_PULLUP);

  ledFx_init();
  ledFx_startupBlink();

  display_init();
  display_splash();

  encoder_init();
  lastPos    = encoder_getPos();
  lastMissed = encoder_getMissed();

  uint8_t bootPins = (digitalRead(CLK_PIN) << 1) | digitalRead(DT_PIN);
  serialOut_banner(ENCODER_NAME, bootPins);
}

void loop() {
  // Sampling runs in the encoder's timer ISR — the loop only drains results.

  // ── Rotation events — drain one detent at a time so each prints ───────────
  int32_t pos = encoder_getPos();
  while (pos != lastPos) {
    bool cw = (pos > lastPos);
    lastPos += cw ? 1 : -1;
    recordRotation(cw);
    ledFx_flash(cw ? CRGB::Red : CRGB::Blue);
    serialOut_rotation(cw);
  }

  // ── Missed-step events — Gray-code violations the decoder couldn't track ──
  // Serial-only on purpose: no LED flash, so measuring misses during a fast
  // spin doesn't add loop work that would itself cause more misses.
  uint32_t missed = encoder_getMissed();
  while (lastMissed != missed) {
    lastMissed++;
    recordMissed();
    serialOut_missed();
  }

  // ── Button — debounced edges; short press = click, long hold = reset ──────
  int swReading = digitalRead(SW_PIN);
  if (swReading != lastSwState && (millis() - lastSwChange) > SW_DEBOUNCE_MS) {
    lastSwChange = millis();
    lastSwState  = swReading;
    if (swReading == LOW) {            // pressed
      pressStart = millis();
      longFired  = false;
    } else if (!longFired) {           // released without a long-press → click
      recordClick();
      ledFx_flash(CRGB::Green);
      serialOut_click();
    }
  }

  // Held past the threshold: reset counters and encoder position once.
  if (lastSwState == LOW && !longFired &&
      (millis() - pressStart) >= LONG_PRESS_MS) {
    longFired = true;
    encoder_reset();
    lastPos    = encoder_getPos();
    lastMissed = encoder_getMissed();   // resync so the missed-drain can't spin
    resetCounts();
    ledFx_flash(CRGB::White);
    serialOut_reset();
  }

  ledFx_tick();
  if (displayDirty) {
    display_update();
    displayDirty = false;
  }
}
