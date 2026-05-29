#include "LedFx.h"
#include "Config.h"

static CRGB          extLeds[NUM_LEDS];
static CRGB          flashColor = CRGB::Black;
static unsigned long flashUntil = 0;

static void setOnboard(bool on) {
  digitalWrite(ONBOARD_PIN, on ? ONBOARD_ON : ONBOARD_OFF);
}

void ledFx_init() {
  pinMode(ONBOARD_PIN, OUTPUT);
  setOnboard(false);
  FastLED.addLeds<WS2812, EXT_PIN, GRB>(extLeds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
}

void ledFx_startupBlink() {
  for (int i = 0; i < 2; i++) {
    extLeds[0] = CRGB::White; FastLED.show(); setOnboard(true);
    delay(250);
    extLeds[0] = CRGB::Black; FastLED.show(); setOnboard(false);
    delay(200);
  }
}

void ledFx_flash(CRGB color) {
  flashColor = color;
  flashUntil = millis() + FLASH_MS;
}

void ledFx_tick() {
  bool active = ((long)(millis() - flashUntil) < 0);
  CRGB target = active ? flashColor : CRGB::Black;
  if (target != extLeds[0]) {
    extLeds[0] = target;
    FastLED.show();
    setOnboard(active);
  }
}
