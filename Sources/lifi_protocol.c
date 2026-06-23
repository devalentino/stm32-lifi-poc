#include "lifi_protocol.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "lifi_receiver.h"
#include "lifi_transmitter.h"

#define LIFI_PACKAGE_MAX_PAYLOAD_SIZE (LIFI_TX_BUFFER_SIZE - TX_PACKAGE_HEADER_BYTES - 1)

static uint8_t calculate_crc(const uint8_t *buffer, uint8_t length) {
  uint8_t crc = 0;

  while (length--) {
    crc ^= *buffer++;
    for (uint8_t i = 0; i < 8; ++i) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
  }

  return crc;
}

static void build_package(uint8_t *destination, PackageType_t type, uint8_t id,
                          const uint8_t *payload, uint8_t payload_length) {
  destination[TX_PACKAGE_PREMBULE_INDEX] = PREAMBULE;
  destination[TX_PACKAGE_START_INDEX] = START_BYTE;
  destination[TX_PACKAGE_PACKAGE_TYPE_INDEX] = (uint8_t)type;
  destination[TX_PACKAGE_ID_INDEX] = id;
  destination[TX_PACKAGE_LENGTH_INDEX] = payload_length;

  if (payload_length > 0) {
    memcpy(destination + TX_PACKAGE_HEADER_BYTES, payload, payload_length);
  }

  uint8_t crc_index = TX_PACKAGE_HEADER_BYTES + payload_length;
  destination[crc_index] = calculate_crc(destination + TX_PACKAGE_PACKAGE_TYPE_INDEX,
                                         payload_length + TX_PACKAGE_HEADER_BYTES - 2);
}

static void transmit_package(LiFi_Socket_t *socket, PackageType_t type, uint8_t id,
                             const uint8_t *payload, uint8_t payload_length) {
  build_package(socket->tx_package, type, id, payload, payload_length);
  LiFi_Transmitter_TransmitBuffer(socket->transmitter, socket->tx_package,
                                  payload_length + TX_PACKAGE_HEADER_BYTES + 1);
}

static void clear_transaction(LiFi_Socket_t *socket) {
  socket->tx_buffer = NULL;
  socket->tx_buffer_length = 0;
  socket->tx_bytes_processed = 0;
  socket->tx_package_id = 0;
  socket->tx_retries_count = 0;

  socket->rx_buffer = NULL;
  socket->rx_bytes_received = 0;
  socket->rx_package_id = 0;
  socket->rx_package_bytes_received = 0;

  socket->transmitter->is_busy = false;
  socket->state = LIFI_SOCKET_IDLE;
}

static void notify_error_and_reset(LiFi_Socket_t *socket) {
  if (socket->on_error_callback != NULL) {
    socket->on_error_callback(LIFI_SOCKET_CONNECTION_ERROR, socket->on_error_callback_context);
  }

  clear_transaction(socket);
}

static uint8_t current_payload_length(const LiFi_Socket_t *socket) {
  uint8_t bytes_remaining = socket->tx_buffer_length - socket->tx_bytes_processed;
  return bytes_remaining < LIFI_PACKAGE_MAX_PAYLOAD_SIZE ? bytes_remaining
                                                         : LIFI_PACKAGE_MAX_PAYLOAD_SIZE;
}

static void send_current_payload(LiFi_Socket_t *socket) {
  uint8_t payload_length = current_payload_length(socket);
  transmit_package(socket, PACKAGE_TYPE_PAYLOAD, socket->tx_package_id,
                   socket->tx_buffer + socket->tx_bytes_processed, payload_length);
  socket->state = LIFI_SOCKET_TRANSMITTING;
}

static void send_next_payload(LiFi_Socket_t *socket) {
  ++socket->tx_package_id;
  send_current_payload(socket);
}

static void send_control(LiFi_Socket_t *socket, PackageType_t type, uint8_t id) {
  transmit_package(socket, type, id, NULL, 0);
  socket->state = LIFI_SOCKET_SENDING_CONTROL;
}

static void send_ack(LiFi_Socket_t *socket) {
  send_control(socket, PACKAGE_TYPE_ACK_READY, socket->rx_package_id);
}

static void send_nak(LiFi_Socket_t *socket, uint8_t package_id) {
  send_control(socket, PACKAGE_TYPE_NAK, package_id);
}

static void send_eot(LiFi_Socket_t *socket) {
  transmit_package(socket, PACKAGE_TYPE_EOT, socket->tx_package_id, NULL, 0);
  socket->state = LIFI_SOCKET_TRANSMITTING;
}

static bool received_package_crc_is_valid(const LiFi_Socket_t *socket) {
  uint8_t payload_length = socket->rx_package[RX_PACKAGE_LENGTH_INDEX];
  uint8_t crc_index = RX_PACKAGE_HEADER_BYTES + payload_length;

  return socket->rx_package[crc_index] ==
         calculate_crc(socket->rx_package + RX_PACKAGE_PACKAGE_TYPE_INDEX,
                       payload_length + RX_PACKAGE_HEADER_BYTES - 1);
}

static bool received_confirmation_is_valid(const LiFi_Socket_t *socket) {
  return socket->rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX] == PACKAGE_TYPE_ACK_READY &&
         socket->rx_package[RX_PACKAGE_ID_INDEX] == socket->tx_package_id &&
         received_package_crc_is_valid(socket);
}

static void retry_current_payload_or_fail(LiFi_Socket_t *socket) {
  ++socket->tx_retries_count;

  if (socket->tx_retries_count >= MAX_TRANSMIT_RETRIES_COUNT) {
    notify_error_and_reset(socket);
    return;
  }

  send_current_payload(socket);
}

static void handle_confirmation(LiFi_Socket_t *socket) {
  if (!received_confirmation_is_valid(socket)) {
    retry_current_payload_or_fail(socket);
    return;
  }

  socket->tx_retries_count = 0;
  socket->tx_bytes_processed += socket->tx_package[TX_PACKAGE_LENGTH_INDEX];

  if (socket->tx_bytes_processed < socket->tx_buffer_length) {
    send_next_payload(socket);
    return;
  }

  send_eot(socket);

  if (socket->on_transmission_success_callback != NULL &&
      socket->on_transmission_success_callback_context != NULL) {
    socket->on_transmission_success_callback(socket->on_transmission_success_callback_context);
  }
}

static void handle_payload(LiFi_Socket_t *socket) {
  uint8_t package_id = socket->rx_package[RX_PACKAGE_ID_INDEX];

  if (!received_package_crc_is_valid(socket)) {
    send_nak(socket, package_id);
    return;
  }

  if (package_id == socket->rx_package_id) {
    send_ack(socket);
    return;
  }

  if (package_id != (uint8_t)(socket->rx_package_id + 1)) {
    send_nak(socket, package_id);
    return;
  }

  uint8_t payload_length = socket->rx_package[RX_PACKAGE_LENGTH_INDEX];
  memcpy(socket->rx_buffer + socket->rx_bytes_received,
         socket->rx_package + RX_PACKAGE_HEADER_BYTES, payload_length);
  socket->rx_bytes_received += payload_length;
  socket->rx_package_id = package_id;
  send_ack(socket);
}

static void handle_eot(LiFi_Socket_t *socket) {
  if (!received_package_crc_is_valid(socket) ||
      socket->rx_package[RX_PACKAGE_ID_INDEX] != socket->rx_package_id) {
    send_nak(socket, socket->rx_package[RX_PACKAGE_ID_INDEX]);
    return;
  }

  socket->state = LIFI_SOCKET_IDLE;

  if (socket->on_receive_success_callback != NULL &&
      socket->on_receive_success_callback_context != NULL) {
    socket->on_receive_success_callback(socket->on_receive_success_callback_context);
  }
}

static void handle_received_package(LiFi_Socket_t *socket) {
  PackageType_t type = (PackageType_t)socket->rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX];

  switch (socket->state) {
  case LIFI_SOCKET_WAITING_CONFIRMATION:
    if (type == PACKAGE_TYPE_ACK_READY || type == PACKAGE_TYPE_NAK) {
      handle_confirmation(socket);
    }
    break;

  case LIFI_SOCKET_RECEIVING:
    if (type == PACKAGE_TYPE_PAYLOAD) {
      handle_payload(socket);
    } else if (type == PACKAGE_TYPE_EOT) {
      handle_eot(socket);
    }
    break;

  default:
    break;
  }
}

static void on_buffer_transmitted(void *context) {
  LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

  switch (socket->state) {
  case LIFI_SOCKET_TRANSMITTING:
    if (socket->tx_package[TX_PACKAGE_PACKAGE_TYPE_INDEX] == PACKAGE_TYPE_EOT) {
      clear_transaction(socket);
      return;
    }

    socket->state = LIFI_SOCKET_WAITING_CONFIRMATION;
    socket->rx_package_bytes_received = 0;
    LiFi_Transmitter_ToConfirmationMode(socket->transmitter);
    break;

  case LIFI_SOCKET_SENDING_CONTROL:
    socket->state = LIFI_SOCKET_RECEIVING;
    break;

  default:
    break;
  }
}

static void on_transmitter_timeout(void *context) {
  LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

  if (socket->state == LIFI_SOCKET_WAITING_CONFIRMATION) {
    retry_current_payload_or_fail(socket);
  }
}

static void on_receiver_timeout(void *context) {
  LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

  if (socket->state == LIFI_SOCKET_RECEIVING || socket->state == LIFI_SOCKET_WAITING_CONFIRMATION ||
      socket->state == LIFI_SOCKET_SENDING_CONTROL) {
    notify_error_and_reset(socket);
  }
}

static void on_byte_received(void *context) {
  LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

  if (socket->state != LIFI_SOCKET_RECEIVING && socket->state != LIFI_SOCKET_WAITING_CONFIRMATION) {
    return;
  }

  if (socket->rx_package_bytes_received == 0 && socket->receiver->rx_byte != START_BYTE) {
    return;
  }

  if (socket->rx_package_bytes_received >= sizeof(socket->rx_package)) {
    socket->rx_package_bytes_received = 0;
    return;
  }

  socket->rx_package[socket->rx_package_bytes_received++] = socket->receiver->rx_byte;

  if (socket->rx_package_bytes_received < RX_PACKAGE_HEADER_BYTES) {
    return;
  }

  uint8_t payload_length = socket->rx_package[RX_PACKAGE_LENGTH_INDEX];
  uint8_t package_length = payload_length + RX_PACKAGE_HEADER_BYTES + 1;
  if (package_length > sizeof(socket->rx_package)) {
    socket->rx_package_bytes_received = 0;
    return;
  }

  if (socket->rx_package_bytes_received < package_length) {
    return;
  }

  handle_received_package(socket);
  socket->rx_package_bytes_received = 0;
}

void LiFi_Socket_Init(LiFi_Socket_t *socket, LiFi_Transmitter_t *transmitter,
                      LiFi_Receiver_t *receiver, LiFi_Socket_onErrorCallback on_error_callback,
                      void *on_error_callback_context,
                      LiFi_Socket_onTransmissionSuccessfulCallback on_transmission_success_callback,
                      void *on_transmission_success_callback_context,
                      LiFi_Socket_onReceiveSuccessfulCallback on_receive_success_callback,
                      void *on_receive_success_callback_context) {
  memset(socket, 0, sizeof(*socket));
  socket->state = LIFI_SOCKET_IDLE;
  socket->on_error_callback = on_error_callback;
  socket->on_error_callback_context = on_error_callback_context;
  socket->on_transmission_success_callback = on_transmission_success_callback;
  socket->on_transmission_success_callback_context = on_transmission_success_callback_context;
  socket->on_receive_success_callback = on_receive_success_callback;
  socket->on_receive_success_callback_context = on_receive_success_callback_context;

  socket->transmitter = transmitter;
  socket->transmitter->on_buffer_transmitted = on_buffer_transmitted;
  socket->transmitter->on_buffer_transmitted_callback_context = socket;
  socket->transmitter->on_timeout_callback = on_transmitter_timeout;
  socket->transmitter->on_timeout_callback_context = socket;

  socket->receiver = receiver;
  socket->receiver->on_byte_received = on_byte_received;
  socket->receiver->on_byte_received_callback_context = socket;
  socket->receiver->on_timeout_callback = on_receiver_timeout;
  socket->receiver->on_timeout_callback_context = socket;
}

bool LiFi_Socket_Send(LiFi_Socket_t *socket, uint8_t *buffer, uint8_t length) {
  if (socket->state != LIFI_SOCKET_IDLE || buffer == NULL || length == 0) {
    return false;
  }

  socket->tx_buffer = buffer;
  socket->tx_buffer_length = length;
  socket->tx_bytes_processed = 0;
  socket->tx_package_id = 0;
  socket->tx_retries_count = 0;

  send_next_payload(socket);
  return true;
}

bool LiFi_Socket_Read(LiFi_Socket_t *socket, uint8_t *buffer) {
  if (socket->state != LIFI_SOCKET_IDLE || buffer == NULL) {
    return false;
  }

  socket->rx_buffer = buffer;
  socket->rx_bytes_received = 0;
  socket->rx_package_id = 0;
  socket->rx_package_bytes_received = 0;
  memset(socket->rx_package, 0, sizeof(socket->rx_package));
  socket->state = LIFI_SOCKET_RECEIVING;

  LiFi_Receiver_ReadBuffer(socket->receiver);
  return true;
}
