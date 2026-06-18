#include "fake_lifi_receiver.h"
#include <stddef.h>

void LiFi_Receiver_TimerCallback(LiFi_Receiver_t *receiver) {
  if (receiver->on_timeout_callback != NULL && receiver->on_timeout_callback_context != NULL) {
    receiver->on_timeout_callback(receiver->on_timeout_callback_context);
  }
}