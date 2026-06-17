#include <stddef.h>
#include "fake_lifi_transmitter.h"

void LiFi_Transmitter_TimerCallback(LiFi_Transmitter_t *transmitter) {
  if (transmitter->on_timeout_callback != NULL && transmitter->on_timeout_callback != NULL) {
    transmitter->on_timeout_callback(transmitter->on_timeout_callback_context);
  }
}