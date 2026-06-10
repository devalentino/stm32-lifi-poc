#ifndef FAKE_LIFI_TRANSPORT_H
#define FAKE_LIFI_TRANSPORT_H

#include "lifi_receiver.h"
#include "lifi_transmitter.h"
#include <stdbool.h>

typedef struct {
    LiFi_Transmitter_t *transmitter;
    LiFi_Receiver_t *receiver;
    uint8_t pending_buffer[LIFI_TX_BUFFER_SIZE];
    uint8_t pending_length;
    uint8_t pending_index;
    bool has_pending_tx;
} Fake_LiFi_Link_t;

void Fake_LiFi_Link_Reset();

void Fake_LiFi_Link_Register(LiFi_Transmitter_t *transmitter, LiFi_Receiver_t *receiver);

void Fake_LiFi_Transmit(Fake_LiFi_Link_t *link);

void Fake_LiFi_RunUntilIdle(void);

void LiFi_Transmitter_TransmitBuffer(LiFi_Transmitter_t *transmitter, const uint8_t *buffer, uint8_t length);

void LiFi_Transmitter_ToTransmitMode(LiFi_Transmitter_t *transmitter);

void LiFi_Transmitter_ToConfirmationMode(LiFi_Transmitter_t *transmitter);

#endif
