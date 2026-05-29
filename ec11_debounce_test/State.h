// Shared application state — counters, last action, dirty flag.
// All event modules (LedFx, Display, SerialOut) read from here.
#pragma once
#include <Arduino.h>

enum LastAction { ACT_NONE, ACT_RIGHT, ACT_LEFT, ACT_CLICK };

extern uint8_t    cycleStep;    // 1..20, rotations only — wraps after 20
extern uint32_t   cwCount;      // total CW rotations
extern uint32_t   ccwCount;     // total CCW rotations
extern uint32_t   clickCount;   // total clicks
extern uint32_t   missedCount;  // suspected skipped detents (Gray-code violations)
extern LastAction lastAction;
extern bool       displayDirty; // set on any event, cleared after display redraw

// Mutators — call exactly one per event in the main loop.
void recordRotation(bool cw);
void recordClick();
void recordMissed();  // a quadrature step the decoder couldn't track
void resetCounts();   // zero every counter (long-press reset)
