// Encoder decoder registry — pluggable methods behind a common interface.
//
// Each EncoderXxx.cpp file implements one decoding strategy and exposes the
// four functions below. Encoder.cpp gathers them into ENC_METHODS[] so the
// main sketch (and the future mode-cycler) can call them generically.
//
// Contract for each method:
//   init()    — claim pins/peripherals, zero internal counters
//   deinit()  — release pins/peripherals (so another method can take over)
//   getPos()  — return signed detent count since init (1 unit = 1 detent)
//   poll()    — called every loop iteration; some methods are no-ops here
#pragma once
#include <Arduino.h>

struct EncoderMethod {
  const char* name;
  void    (*init)();
  void    (*deinit)();
  int32_t (*getPos)();
  void    (*poll)();
};

// Prototypes for each implementation. Defined in EncoderPolledBux.cpp etc.
void    polledBux_init();   void polledBux_deinit();
int32_t polledBux_getPos(); void polledBux_poll();

void    rawBux_init();      void rawBux_deinit();
int32_t rawBux_getPos();    void rawBux_poll();

void    qem_init();         void qem_deinit();
int32_t qem_getPos();       void qem_poll();

extern EncoderMethod ENC_METHODS[];
extern const uint8_t ENC_METHOD_COUNT;
extern uint8_t       activeEncoderIdx;

inline EncoderMethod& activeEnc() { return ENC_METHODS[activeEncoderIdx]; }
