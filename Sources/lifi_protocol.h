#ifndef LIFI_RECEIVER_H
#define LIFI_RECEIVER_H

#include <stdint.h>
typedef struct {
    LiFi_Transmitter_t  transmitter;
    LiFi_Receiver_t     receiver;

    uint8_t            *tx_buffer;
    uint8_t             tx_buffer_length;
    uint8_t.            tx_bytes_processed;
    
    uint8_t            *tx_package;
    uint8_t             tx_id;

    uint8_t            *rx_buffer;
} LiFi_Socket_t;

bool LiFi_Send(LiFi_Socket_t socket, uint8_t *buffer, uint8_t length);

void LiFi_Read(LiFi_Socket_t socket, uint8_t buffer);

#endif