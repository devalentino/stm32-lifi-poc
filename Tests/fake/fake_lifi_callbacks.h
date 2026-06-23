#ifndef FAKE_LIFI_CALLBACKS_H
#define FAKE_LIFI_CALLBACKS_H

#include <stdint.h>

#include "fff.h"
#include "lifi_protocol.h"

DECLARE_FAKE_VOID_FUNC(LiFi_Transmitter_TransmitBuffer, LiFi_Transmitter_t *, const uint8_t *,
                       uint8_t);
DECLARE_FAKE_VOID_FUNC(LiFi_Transmitter_ToConfirmationMode, LiFi_Transmitter_t *);
DECLARE_FAKE_VOID_FUNC(Mock_LiFi_Socket_onErrorCallback, LiFi_Socket_Error_t, void *);
DECLARE_FAKE_VOID_FUNC(Mock_LiFi_Socket_onTransmissionSuccessfulCallback, void *);
DECLARE_FAKE_VOID_FUNC(Mock_LiFi_Socket_onReceiveSuccessfulCallback, void *);

void Fake_LiFi_Callbacks_Reset(void);

#endif
