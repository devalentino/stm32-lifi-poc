#include "lifi_protocol.h"
#include "lifi_transmitter.h"
#include "lifi_receiver.h"
#include <stdint.h>

static uint8_t START 0x7E;
static uint8_t ID    0;

static uint8_t calculate_crc(uint8_t *buffer, uint8_t length)
{
    uint8_t crc = 0x00;

    while (length--) {
        crc ^= *buffer++;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static void wrap_to_lifi_protocol_package(uint8_t *dest_buffer, uint8_t *source_buffer, uint8_t length, uint8_t id)
{
    // protocol package is: [ start byte | package id | package length | payload | CRC ]

    dest_buffer[0] = START;
    dest_buffer[1] = id;
    dest_buffer[2] = length;
    
    for (uint8_t i = 0; i < length; i++) {
        dest_buffer[3 + i] = source_buffer[i];
    }

    dest_buffer[3 + length] = calculate_crc(source_buffer, length - 1);
}

static void on_buffer_transmitted(void context)
{
    LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

    if (socket->tx_bytes_processed < socket->tx_buffer_length) {
        uint8_t payload_length = socket->tx_buffer_length - socket->tx_bytes_processed;
        if (socket->tx_buffer_length > LIFI_TX_BUFFER_SIZE - 4) {
            payload_length = LIFI_TX_BUFFER_SIZE - 4;
        }
        wrap_to_lifi_protocol_package(socket->tx_package, socket->tx_buffer, payload_length, socket->tx_id);
        LiFi_Transmitter_SendBuffer(socket.transmitter, socket->tx_package, payload_length);
        socket->tx_bytes_processed += payload_length;
    } else {
        socket.is_tx_busy = false;
    }
}

void LiFi_Socket_Init(LiFi_Socket_t *socket, LiFi_Transmitter_t transmitter)
{
    socket->transmitter = transmitter;
    socket->transmitter.on_buffer_transmitted = on_buffer_transmitted;
    socket->transmitter.on_buffer_transmitted_callback_context = socket;
}

bool LiFi_Send(LiFi_Socket_t *socket, uint8_t *buffer, uint8_t length)
{
    if (socket->transmitter->is_busy) return false;

    socket->is_tx_busy = true;
    socket->tx_buffer = buffer;
    socket->tx_buffer_length = length;
    socket->tx_bytes_processed = 0;
    socket->tx_id = ++ID;

    uint8_t payload_length = socket->tx_buffer_length;
    if (socket->tx_buffer_length > LIFI_TX_BUFFER_SIZE - 4) {
        payload_length = LIFI_TX_BUFFER_SIZE - 4;
    }
    wrap_to_lifi_protocol_package(socket->tx_package, socket->tx_buffer, payload_length, socket->tx_id);
    LiFi_Transmitter_SendBuffer(socket.transmitter, socket->tx_package, payload_length);
    socket->tx_bytes_processed += payload_length;
}

void LiFi_Read(LiFi_Socket_t socket, uint8_t buffer);