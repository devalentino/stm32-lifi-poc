#ifndef FAKE_LIFI_TRANSPORT_H
#define FAKE_LIFI_TRANSPORT_H

#include "lifi_receiver.h"
#include "lifi_transmitter.h"

typedef struct {
    LiFi_Transmitter_t *transmitter;
    LiFi_Receiver_t *receiver;
} Fake_LiFi_Link_t;

void Fake_LiFi_Link_Reset();

void Fake_LiFi_Link_Register(LiFi_Transmitter_t *transmitter, LiFi_Receiver_t *receiver);

void LiFi_Transmitter_TransmitBuffer(LiFi_Transmitter_t *transmitter, const uint8_t *buffer, uint8_t length);

void LiFi_Transmitter_ToConfirmationMode(LiFi_Transmitter_t *transmitter);

#endif