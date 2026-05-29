#include "Encoder.h"

// Registry of all available decoder methods. Index 0 is the default at boot.
EncoderMethod ENC_METHODS[] = {
  { "Polled+Confirm Buxtronix", polledBux_init, polledBux_deinit, polledBux_getPos, polledBux_poll },
  { "ISR Buxtronix (no filter)", rawBux_init,    rawBux_deinit,    rawBux_getPos,    rawBux_poll    },
  { "Polled QEM (quad steps)",   qem_init,       qem_deinit,       qem_getPos,       qem_poll       },
};
const uint8_t ENC_METHOD_COUNT = sizeof(ENC_METHODS) / sizeof(EncoderMethod);
uint8_t       activeEncoderIdx = 0;
