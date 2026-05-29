#include "Display.h"
#include "Config.h"
#include "State.h"
#include "Encoder.h"
#include <Wire.h>
#include <U8g2lib.h>

// VCOMH0 init variant fixes a column-0 ghost on some SSD1306 clones.
// If your panel still shows a noisy left column, swap to _NONAME_ or _ALT0_.
static U8G2_SSD1306_128X64_VCOMH0_F_HW_I2C oled(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
static bool    present  = false;
static uint8_t oledAddr = 0;

// Every responding 7-bit address from the last scan (room for a full 8-device
// I2C hub). display_init() scans once; the OLED detector and any future
// multi-device code both read from here instead of re-probing the bus.
static uint8_t i2cAddrs[8];
static uint8_t i2cCount = 0;

void display_scanI2C() {
  Serial.print("I2C scan on SDA=GPIO");
  Serial.print(SDA_PIN);
  Serial.print(" SCL=GPIO");
  Serial.print(SCL_PIN);
  Serial.println(":");
  i2cCount = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      if (i2cCount < sizeof(i2cAddrs)) i2cAddrs[i2cCount++] = addr;
      Serial.print("  device at 0x");
      if (addr < 0x10) Serial.print('0');
      Serial.println(addr, HEX);
    }
  }
  if (i2cCount == 0) Serial.println("  (no devices responded)");
}

// Pick the first SSD1306-range address the scan actually found (0x3C or 0x3D)
// so a panel strapped to 0x3D is driven correctly instead of failing silently.
static bool detectOled() {
  for (uint8_t i = 0; i < i2cCount; i++) {
    if (i2cAddrs[i] == 0x3C || i2cAddrs[i] == 0x3D) {
      oledAddr = i2cAddrs[i];
      return true;
    }
  }
  return false;
}

void display_init() {
  Wire.begin(SDA_PIN, SCL_PIN);
  display_scanI2C();
  present = detectOled();
  if (present) {
    oled.setI2CAddress(oledAddr << 1);   // U8g2 wants the 8-bit (shifted) form
    oled.begin();
    oled.setBusClock(400000);
  }
}

bool    display_isPresent() { return present; }
uint8_t display_oledAddr()  { return oledAddr; }

void display_splash() {
  if (!present) return;
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

void display_update() {
  if (!present) return;
  oled.clearBuffer();

  // Header — the active debounce method
  oled.setFont(u8g2_font_5x8_tr);
  oled.drawStr(2, 8, ENCODER_NAME);
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
