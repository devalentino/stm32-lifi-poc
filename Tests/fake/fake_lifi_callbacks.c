#include "fake_lifi_callbacks.h"

DEFINE_FFF_GLOBALS;

DEFINE_FAKE_VOID_FUNC3(LiFi_Transmitter_TransmitBuffer, LiFi_Transmitter_t *, const uint8_t *,
                       uint8_t);
DEFINE_FAKE_VOID_FUNC1(LiFi_Transmitter_ToConfirmationMode, LiFi_Transmitter_t *);
DEFINE_FAKE_VOID_FUNC2(Mock_LiFi_Socket_onErrorCallback, LiFi_Socket_Error_t, void *);
DEFINE_FAKE_VOID_FUNC1(Mock_LiFi_Socket_onTransmissionSuccessfulCallback, void *);
DEFINE_FAKE_VOID_FUNC1(Mock_LiFi_Socket_onReceiveSuccessfulCallback, void *);

void Fake_LiFi_Callbacks_Reset(void) {
  RESET_FAKE(LiFi_Transmitter_TransmitBuffer);
  RESET_FAKE(LiFi_Transmitter_ToConfirmationMode);
  RESET_FAKE(Mock_LiFi_Socket_onErrorCallback);
  RESET_FAKE(Mock_LiFi_Socket_onTransmissionSuccessfulCallback);
  RESET_FAKE(Mock_LiFi_Socket_onReceiveSuccessfulCallback);
  FFF_RESET_HISTORY();
}
