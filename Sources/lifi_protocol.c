#include "lifi_protocol.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "lifi_receiver.h"
#include "lifi_transmitter.h"

static uint8_t calculate_crc(uint8_t *buffer, uint8_t length) {
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

static void wrap_to_lifi_protocol_package(uint8_t *dest_buffer, uint8_t *source_buffer,
                                          uint8_t package_type, uint8_t id, uint8_t length) {
  // protocol package is:
  // [ preambule | start byte | package type | package id | package length | payload | CRC ]

  dest_buffer[TX_PACKAGE_PREMBULE_INDEX] = PREAMBULE;
  dest_buffer[TX_PACKAGE_START_INDEX] = START_BYTE;
  dest_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX] = package_type;
  dest_buffer[TX_PACKAGE_ID_INDEX] = id;
  dest_buffer[TX_PACKAGE_LENGTH_INDEX] = length;

  memcpy(dest_buffer + TX_PACKAGE_HEADER_BYTES, source_buffer, length);

  uint8_t crc_index = TX_PACKAGE_HEADER_BYTES + length;
  dest_buffer[crc_index] = calculate_crc(dest_buffer + TX_PACKAGE_PACKAGE_TYPE_INDEX,
                                         length + TX_PACKAGE_HEADER_BYTES - 2);
}

static void reset_socket(LiFi_Socket_t *socket) {
  socket->tx_buffer = NULL;
  socket->tx_buffer_length = 0;
  socket->tx_bytes_processed = 0;
  socket->tx_package_id = 0;
  socket->tx_retries_count = 0;
  socket->is_tx_confirmation_required = false;

  socket->rx_buffer = NULL;
  socket->rx_package_id = 0;
  socket->rx_package_bytes_received = 0;

  socket->transmitter->is_busy = false;
  socket->is_busy = false;
}

static void setup_transmission(LiFi_Socket_t *socket, uint8_t *buffer, PackageType_t package_type,
                               uint8_t length) {
  socket->tx_buffer = buffer;
  socket->tx_buffer_length = length;
  socket->tx_bytes_processed = 0;
  socket->tx_retries_count = 0;
  socket->is_tx_confirmation_required = false;
  socket->tx_package_type = (uint8_t)package_type;
  socket->tx_package_id = 0;
}

static void transmit_package(LiFi_Socket_t *socket) {
  if (socket->tx_package_type == PACKAGE_TYPE_ACK || socket->tx_package_type == PACKAGE_TYPE_NAK) {
    socket->tx_package_id = socket->rx_package_id;
  } else {
    socket->tx_package_id++;
  }

  uint8_t payload_length = socket->tx_buffer_length - socket->tx_bytes_processed;
  if (socket->tx_buffer_length > LIFI_TX_BUFFER_SIZE - TX_PACKAGE_HEADER_BYTES - 1) {
    payload_length = LIFI_TX_BUFFER_SIZE - TX_PACKAGE_HEADER_BYTES - 1;
  }
  wrap_to_lifi_protocol_package(socket->tx_package, socket->tx_buffer + socket->tx_bytes_processed,
                                socket->tx_package_type, socket->tx_package_id, payload_length);

  uint8_t package_length = payload_length + TX_PACKAGE_HEADER_BYTES + 1;
  LiFi_Transmitter_TransmitBuffer(socket->transmitter, socket->tx_package, package_length);
}

static void on_buffer_transmitted(void *context) {
  LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

  if (socket->rx_buffer == NULL) {
    // receiver is empty, means payload had been sent
    socket->is_tx_confirmation_required = true;
    LiFi_Transmitter_ToConfirmationMode(socket->transmitter);
  } else {
    // confirmation had been sent
  }
}

static void on_transmitter_timeout(void *context) {
  LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

  if (!socket->is_tx_confirmation_required)
    return;

  if (socket->on_error_callback != NULL) {
    socket->on_error_callback(LIFI_SOCKET_CONNECTION_ERROR, socket);
  }
  reset_socket(socket);
}

static void on_receiver_timer(void *context) {
  LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

  if (socket->on_error_callback != NULL) {
    socket->on_error_callback(LIFI_SOCKET_CONNECTION_ERROR, socket);
  }
  reset_socket(socket);
}

static void ack(LiFi_Socket_t *socket) {
  setup_transmission(socket, NULL, PACKAGE_TYPE_ACK, 0);
  transmit_package(socket);
}

static void nak(LiFi_Socket_t *socket) {
  setup_transmission(socket, NULL, PACKAGE_TYPE_NAK, 0);
  transmit_package(socket);
}

static void eot(LiFi_Socket_t *socket) {
  setup_transmission(socket, NULL, PACKAGE_TYPE_EOT, 0);
  transmit_package(socket);
}

static void on_package_received_process_payload(LiFi_Socket_t *socket) {

  uint8_t package_bytes_received = socket->rx_package_bytes_received;
  uint8_t package_id = socket->rx_package[RX_PACKAGE_ID_INDEX];
  socket->rx_package_bytes_received = 0;

  if (socket->rx_package_id == 0) {
    socket->rx_package_id = package_id;
  }

  if (socket->rx_package_id != package_id) {
    nak(socket);
    return;
  }

  uint8_t crc = socket->rx_package[package_bytes_received - 1];
  uint8_t payload_length = socket->rx_package[RX_PACKAGE_LENGTH_INDEX];
  if (crc != calculate_crc(socket->rx_package + RX_PACKAGE_PACKAGE_TYPE_INDEX,
                           payload_length + RX_PACKAGE_HEADER_BYTES - 1)) {
    nak(socket);
    return;
  }

  memcpy(socket->rx_buffer, socket->rx_package + RX_PACKAGE_HEADER_BYTES, payload_length);

  ack(socket);
}

static bool is_received_received_package_confirmed(LiFi_Socket_t *socket) {
  uint8_t package_id = socket->rx_package[RX_PACKAGE_ID_INDEX];

  if (socket->tx_package_id != package_id) {
    return false;
  }

  uint8_t package_type = socket->rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX];
  if (package_type != PACKAGE_TYPE_ACK) {
    return false;
  }

  uint8_t crc = socket->rx_package[socket->rx_package_bytes_received - 1];
  uint8_t payload_length = socket->rx_package[RX_PACKAGE_LENGTH_INDEX];

  if (crc != calculate_crc(socket->rx_package + RX_PACKAGE_PACKAGE_TYPE_INDEX,
                           payload_length + RX_PACKAGE_HEADER_BYTES - 1)) {
    return false;
  }

  return true;
}

void on_package_received_process_confirmation(LiFi_Socket_t *socket) {
  if (!socket->is_tx_confirmation_required)
    return;

  socket->is_tx_confirmation_required = false;

  if (is_received_received_package_confirmed(socket)) {
    uint8_t payload_length = socket->tx_package[TX_PACKAGE_LENGTH_INDEX];
    socket->tx_bytes_processed += payload_length;

    if (socket->tx_bytes_processed < socket->tx_buffer_length) {
      transmit_package(socket);
    } else {
      eot(socket);

      if (socket->on_transmission_success_callback != NULL)
        socket->on_transmission_success_callback(socket);
    }
  } else {
    socket->tx_retries_count++;

    if (socket->tx_retries_count < MAX_TRANSMIT_RETRIES_COUNT) {
      transmit_package(socket);
    } else {
      if (socket->on_error_callback != NULL) {
        socket->on_error_callback(LIFI_SOCKET_CONNECTION_ERROR, socket);
      }
      reset_socket(socket);
    }
  }
}

void on_package_received_process_end_of_transmission(LiFi_Socket_t *socket) {
  if (socket->on_receive_success_callback != NULL)
    socket->on_receive_success_callback(socket);
}

void on_package_received(LiFi_Socket_t *socket) {
  uint8_t package_type = socket->rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX];
  switch (package_type) {
  case PACKAGE_TYPE_PAYLOAD:
    on_package_received_process_payload(socket);
    break;
  case PACKAGE_TYPE_ACK:
  case PACKAGE_TYPE_NAK:
    on_package_received_process_confirmation(socket);
    break;
  case PACKAGE_TYPE_EOT:
    on_package_received_process_end_of_transmission(socket);
    break;
  default:
    break;
  }
}

static void on_byte_received(void *context) {
  LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

  if (socket->rx_package_bytes_received == 0 && socket->receiver->rx_byte != START_BYTE) {
    return;
  }

  socket->rx_package[socket->rx_package_bytes_received++] = socket->receiver->rx_byte;

  if (socket->rx_package_bytes_received < RX_PACKAGE_HEADER_BYTES) {
    return;
  }

  uint8_t payload_length = socket->rx_package[RX_PACKAGE_LENGTH_INDEX];
  uint8_t package_length = payload_length + RX_PACKAGE_HEADER_BYTES + 1;
  uint8_t is_full_package_received = socket->rx_package_bytes_received >= package_length;
  if (!is_full_package_received) {
    return;
  }

  on_package_received(socket);
}

void LiFi_Socket_Init(LiFi_Socket_t *socket, LiFi_Transmitter_t *transmitter,
                      LiFi_Receiver_t *receiver, LiFi_Socket_onErrorCallback on_error_callback,
                      LiFi_Socket_onTransmissionSuccessfulCallback on_transmission_success_callback,
                      LiFi_Socket_onReceiveSuccessfulCallback on_receive_success_callback) {
  socket->is_busy = false;
  socket->on_error_callback = on_error_callback;
  socket->on_transmission_success_callback = on_transmission_success_callback;
  socket->on_receive_success_callback = on_receive_success_callback;

  socket->is_tx_confirmation_required = false;

  socket->transmitter = transmitter;
  socket->transmitter->on_buffer_transmitted = on_buffer_transmitted;
  socket->transmitter->on_buffer_transmitted_callback_context = socket;
  socket->transmitter->on_timeout_callback = on_transmitter_timeout;
  socket->transmitter->on_timeout_callback_context = socket;

  socket->receiver = receiver;
  socket->receiver->on_byte_received = on_byte_received;
  socket->receiver->on_byte_received_callback_context = socket;
  socket->receiver->on_timeout_callback = on_receiver_timer;
  socket->receiver->on_timeout_callback_context = socket;
}

bool LiFi_Socket_Send(LiFi_Socket_t *socket, uint8_t *buffer, uint8_t length) {
  if (socket->is_busy)
    return false;

  socket->is_busy = true;
  setup_transmission(socket, buffer, PACKAGE_TYPE_PAYLOAD, length);
  transmit_package(socket);
  return true;
}

bool LiFi_Socket_Read(LiFi_Socket_t *socket, uint8_t *buffer) {
  if (socket->is_busy)
    return false;

  socket->is_busy = true;
  socket->rx_buffer = buffer;
  memset(socket->rx_package, 0, sizeof(socket->rx_package));
  socket->rx_package_bytes_received = 0;
  socket->rx_package_id = 0;

  LiFi_Receiver_ReadBuffer(socket->receiver);
  return true;
}
