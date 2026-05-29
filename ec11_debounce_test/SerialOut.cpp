#include "SerialOut.h"
#include "Config.h"
#include "State.h"
#include "Display.h"

static void printZeroPad(uint32_t n) {
  if (n < 10) Serial.print('0');
  Serial.print(n);
}

void serialOut_banner(const char* methodName, uint8_t pinsAtBoot) {
  Serial.println("================================");
  Serial.println(" EC11 Debounce Test");
  Serial.print  (" Method: ");
  Serial.println(methodName);
  Serial.print  (" CLK: GPIO");  Serial.print(CLK_PIN);
  Serial.print  ("   DT: GPIO"); Serial.print(DT_PIN);
  Serial.print  ("   SW: GPIO"); Serial.println(SW_PIN);
  Serial.print  (" External LED: GPIO"); Serial.println(EXT_PIN);
  Serial.print  (" Onboard LED:  GPIO"); Serial.print(ONBOARD_PIN);
  Serial.println(" (mirror)");
  Serial.print  (" OLED: ");
  if (display_isPresent()) {
    Serial.print("detected at 0x");
    Serial.println(display_oledAddr(), HEX);
  } else {
    Serial.println("not found");
  }
  Serial.println("================================");
  Serial.print  ("Idle pin state (should be 3): ");
  Serial.println(pinsAtBoot);
  Serial.println("Rotate or press encoder...");
  Serial.println();
}

void serialOut_rotation(bool cw) {
  Serial.print(cw ? "--> RIGHT - STEP " : "<-- LEFT  - STEP ");
  printZeroPad(cycleStep);
  Serial.println();
}

void serialOut_click() {
  Serial.print("(*) CLICK - CLK ");
  printZeroPad(clickCount);
  Serial.println();
}
