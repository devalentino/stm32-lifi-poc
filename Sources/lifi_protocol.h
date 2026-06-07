#ifndef LIFI_PROTOCOL_H
#define LIFI_PROTOCOL_H

#include "lifi_receiver.h"
#include "lifi_transmitter.h"
#include <stdint.h>

typedef struct {
    LiFi_Transmitter_t  *transmitter;
    LiFi_Receiver_t     *receiver;

    uint8_t            *tx_buffer;
    uint8_t             tx_buffer_length;
    uint8_t             tx_bytes_processed;
    uint8_t            *tx_package;
    uint8_t             tx_package_id;

    uint8_t            *rx_buffer;
    uint8_t            *rx_package;
    uint8_t             rx_package_id;
    uint8_t             rx_package_bytes_received;
} LiFi_Socket_t;

void LiFi_Socket_Init(LiFi_Socket_t *socket, LiFi_Transmitter_t *transmitter, LiFi_Receiver_t *receiver);

void LiFi_Socket_Send(LiFi_Socket_t *socket, uint8_t *buffer, uint8_t length);

void LiFi_Socket_Read(LiFi_Socket_t *socket, uint8_t *buffer);

#endif