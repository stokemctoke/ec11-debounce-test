#include "State.h"

uint8_t    cycleStep    = 0;
uint32_t   cwCount      = 0;
uint32_t   ccwCount     = 0;
uint32_t   clickCount   = 0;
LastAction lastAction   = ACT_NONE;
bool       displayDirty = false;

void recordRotation(bool cw) {
  cycleStep = (cycleStep % 20) + 1;   // 1..20, wraps
  if (cw) {
    cwCount++;
    lastAction = ACT_RIGHT;
  } else {
    ccwCount++;
    lastAction = ACT_LEFT;
  }
  displayDirty = true;
}

void recordClick() {
  clickCount++;
  lastAction = ACT_CLICK;
  displayDirty = true;
}
