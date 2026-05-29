// LED flash effects — external WS2812 + onboard blue LED.
#pragma once
#include <Arduino.h>
#include <FastLED.h>

void ledFx_init();
void ledFx_startupBlink();
void ledFx_flash(CRGB color);    // trigger a timed flash
void ledFx_tick();               // call every loop — manages flash decay
