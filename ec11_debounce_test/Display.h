// SSD1306 OLED — auto-detected at boot. All functions no-op if not present.
#pragma once
#include <Arduino.h>

void    display_init();      // Wire.begin + scan + OLED init if found
void    display_scanI2C();   // diagnostic — prints every I2C device to Serial
bool    display_isPresent();
uint8_t display_oledAddr();  // 7-bit address the OLED answered on (0 if none)
void display_splash();      // boot splash
void display_update();      // redraw from State.h values
