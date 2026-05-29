// Formatted serial output — banner, rotation lines, click lines.
#pragma once
#include <Arduino.h>

void serialOut_banner(const char* methodName, uint8_t pinsAtBoot);
void serialOut_rotation(bool cw);
void serialOut_click();
