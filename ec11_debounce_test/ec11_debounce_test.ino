// EC11 debounce test — Tenstar Robot ESP32-C3 Super-Mini
//
// Hardware (see Config.h for pin assignments):
//   Encoder         CLK → GPIO0, DT → GPIO1, SW → GPIO3, common → GND
//   External WS2812 data → GPIO10, 5V, GND
//   Onboard LED     GPIO8 (single blue, active LOW)
//   I2C OLED (opt.) SDA → GPIO6, SCL → GPIO7, VCC → 3.3V, GND → GND
//
// File map:
//   Config.h            — pin and timing constants
//   State.cpp           — shared counters + lastAction (recordRotation/Click)
//   LedFx.cpp           — external WS2812 + onboard LED flash logic
//   Display.cpp         — SSD1306 OLED detection, splash, redraw
//   SerialOut.cpp      — formatted Serial banner + event lines
//   Encoder.cpp         — registry of decoder methods + active-method pointer
//   EncoderPolledBux.cpp — DEFAULT: polled + confirmed Buxtronix (robust)
//   EncoderRawBux.cpp    — ISR Buxtronix, no software filter (fails on bouncy EC11)
//   EncoderQEM.cpp       — raw quadrature event matrix (4 counts/detent)
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
static int            lastSwState = HIGH;
static unsigned long  lastSwChange = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);   // give USB-CDC time to (re)connect after upload/reset

  pinMode(SW_PIN, INPUT_PULLUP);

  ledFx_init();
  ledFx_startupBlink();

  display_init();
  display_splash();

  activeEnc().init();
  lastPos = activeEnc().getPos();

  uint8_t bootPins = (digitalRead(CLK_PIN) << 1) | digitalRead(DT_PIN);
  serialOut_banner(activeEnc().name, bootPins);
}

void loop() {
  activeEnc().poll();

  // ── Rotation events — drain one detent at a time so each prints ───────────
  int32_t pos = activeEnc().getPos();
  while (pos != lastPos) {
    bool cw = (pos > lastPos);
    lastPos += cw ? 1 : -1;
    recordRotation(cw);
    ledFx_flash(cw ? CRGB::Red : CRGB::Blue);
    serialOut_rotation(cw);
  }

  // ── Button — debounced falling edge ───────────────────────────────────────
  int swReading = digitalRead(SW_PIN);
  if (swReading != lastSwState && (millis() - lastSwChange) > SW_DEBOUNCE_MS) {
    lastSwChange = millis();
    if (swReading == LOW) {
      recordClick();
      ledFx_flash(CRGB::Green);
      serialOut_click();
    }
    lastSwState = swReading;
  }

  ledFx_tick();
  if (displayDirty) {
    display_update();
    displayDirty = false;
  }
}
