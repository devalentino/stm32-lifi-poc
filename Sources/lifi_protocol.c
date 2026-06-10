#include "lifi_protocol.h"
#include "lifi_transmitter.h"
#include "lifi_receiver.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define PREAMBULE 0x55;
#define ACK       0x56;
#define NAK       0x57;

static uint8_t PACKAGE_ID = 0;

static uint8_t get_package_id()
{
    ++PACKAGE_ID;

    if (PACKAGE_ID == 0) ++PACKAGE_ID;
    return PACKAGE_ID;
}

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

static void wrap_to_lifi_protocol_package(uint8_t *dest_buffer, uint8_t *source_buffer, uint8_t length, uint8_t id, bool preambule)
{
    uint8_t index = 0;
    if (preambule) {
        dest_buffer[index++] = PREAMBULE;
    }


    // protocol package is: [ start byte | package id | package length | payload | CRC ]

    dest_buffer[index++] = START_BYTE;
    dest_buffer[index++] = id;
    dest_buffer[index++] = length;
    
    for (uint8_t i = 0; i < length; i++) {
        dest_buffer[index++] = source_buffer[i];
    }

    dest_buffer[index] = calculate_crc(source_buffer, length);
}

static void wait_for_confirmation(LiFi_Socket_t *socket)
{
    socket->is_tx_confirmation_required = true;
    LiFi_Transmitter_ToConfirmationMode(socket->transmitter);
}

static void process_package_confirmation(LiFi_Socket_t *socket)
{
    uint8_t package_id = socket->rx_package[1];
    if (socket->rx_package_id != package_id) {
        // confirmation for wrong package
    }

    uint8_t crc = socket->rx_package[socket->rx_package_bytes_received - 1];
    uint8_t payload_length = socket->rx_package[2];
    if (crc != calculate_crc(socket->rx_package + 3, payload_length)) {
        // corrupted confirmation package
    }

    
}

static void on_buffer_transmitted(void *context)
{
    LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

    if (socket->is_tx_confirmation_required) return;

    if (socket->tx_bytes_processed < socket->tx_buffer_length) {
        uint8_t payload_length = socket->tx_buffer_length - socket->tx_bytes_processed;
        if (socket->tx_buffer_length > LIFI_TX_BUFFER_SIZE - 4) {
            payload_length = LIFI_TX_BUFFER_SIZE - 4;
        }
        wrap_to_lifi_protocol_package(socket->tx_package, socket->tx_buffer, payload_length, socket->tx_package_id, false);
        uint8_t package_length = payload_length + 4;
        LiFi_Transmitter_TransmitBuffer(socket->transmitter, socket->tx_package, package_length);
        socket->tx_bytes_processed += payload_length;
    } else {
        wait_for_confirmation(socket);
    }
}

void on_package_received(LiFi_Socket_t *socket) {
    if (socket->is_tx_confirmation_required) {
        process_package_confirmation(socket);
    }

    // process regular consumption
    uint8_t package_id = socket->rx_package[1];
    if (socket->rx_package_id == 0) {
        socket->rx_package_id = package_id;
    }

    if (socket->rx_package_id != package_id) {
        LiFi_Socket_Nak(socket);
    }

    uint8_t crc = socket->rx_package[socket->rx_package_bytes_received - 1];
    uint8_t payload_length = socket->rx_package[2];
    if (crc != calculate_crc(socket->rx_package + 3, payload_length)) {
        LiFi_Socket_Nak(socket);
    }

    for (uint8_t i = 0; i < payload_length; i++) {
        (*socket->rx_buffer++) = socket->rx_package[i + 3];
    }

    LiFi_Socket_Ack(socket);
}

static void on_byte_received(void *context) {
    LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

    if(socket->rx_package_bytes_received == 0 && socket->receiver->rx_byte != START_BYTE) {
        return;
    }

    socket->rx_package[socket->rx_package_bytes_received++] = socket->receiver->rx_byte;

    if (socket->rx_package_bytes_received < 3) {
        return;
    }

    uint8_t payload_length = socket->rx_package[2];
    uint8_t package_length = payload_length + 4;
    uint8_t is_full_package_received = socket->rx_package_bytes_received >= package_length;
    if (!is_full_package_received)
    {
        return;
    }

    on_package_received(socket);
}

void LiFi_Socket_Init(LiFi_Socket_t *socket, LiFi_Transmitter_t *transmitter, LiFi_Receiver_t *receiver)
{
    socket->is_busy = false;
    socket->is_tx_confirmation_required = false;

    socket->transmitter = transmitter;
    socket->transmitter->on_buffer_transmitted = on_buffer_transmitted;
    socket->transmitter->on_buffer_transmitted_callback_context = socket;

    socket->receiver = receiver;
    socket->receiver->on_byte_received = on_byte_received;
    socket->receiver->on_byte_received_callback_context = socket;
}

void LiFi_Socket_Send(LiFi_Socket_t *socket, uint8_t *buffer, uint8_t length)
{
    if (socket->is_busy) return;

    socket->is_busy = true;
    socket->tx_buffer = buffer;
    socket->tx_buffer_length = length;
    socket->tx_bytes_processed = 0;
    socket->tx_retries_count = 0;
    socket->is_tx_confirmation_required = false;
    socket->tx_package_id = get_package_id();

    uint8_t payload_length = socket->tx_buffer_length;
    if (socket->tx_buffer_length > LIFI_TX_BUFFER_SIZE - 5) {
        payload_length = LIFI_TX_BUFFER_SIZE - 5;
    }
    wrap_to_lifi_protocol_package(socket->tx_package, socket->tx_buffer, payload_length, socket->tx_package_id, true);

    uint8_t package_length = payload_length + 5;
    LiFi_Transmitter_TransmitBuffer(socket->transmitter, socket->tx_package, package_length);
    socket->tx_bytes_processed += payload_length;
}

void LiFi_Socket_Read(LiFi_Socket_t *socket, uint8_t *buffer)
{
    socket->rx_buffer = buffer;
    memset(socket->rx_package, 0, sizeof(socket->rx_package));
    socket->rx_package_bytes_received = 0;
    socket->rx_package_id = 0;

    LiFi_Receiver_ReadBuffer(socket->receiver);
}

void LiFi_Socket_Ack(LiFi_Socket_t *socket)
{
  // TODO: implement ACK
}

void LiFi_Socket_Nak(LiFi_Socket_t *socket)
{
  // TODO: implement NAK
}