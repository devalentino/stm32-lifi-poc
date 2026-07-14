#include "lifi_protocol_internal.h"

#define LIFI_PACKAGE_MAX_PAYLOAD_SIZE (LIFI_TX_BUFFER_SIZE - TX_PACKAGE_HEADER_BYTES - 1)

void LiFi_Protocol_TransmitPackage(LiFi_Socket_t *socket, PackageType_t type, uint8_t id,
                                   const uint8_t *payload, uint8_t payload_length) {
  LiFi_Protocol_BuildPackage(socket->tx_package, type, id, payload, payload_length);
  LiFi_Transmitter_TransmitBuffer(socket->transmitter, socket->tx_package,
                                  payload_length + TX_PACKAGE_HEADER_BYTES + 1);
}

void LiFi_Protocol_SendControl(LiFi_Socket_t *socket, PackageType_t type, uint8_t id) {
  LiFi_Protocol_TransmitPackage(socket, type, id, NULL, 0);

  socket->state = LIFI_SOCKET_SENDING_CONTROL;
  if (type == PACKAGE_TYPE_ACK_BUSY || type == PACKAGE_TYPE_BUSY) {
    socket->state = LIFI_SOCKET_SENDING_CONTROL_BUSY;
  }
}

static void send_payload(LiFi_Socket_t *socket) {
  uint8_t bytes_remaining = socket->tx_buffer_length - socket->tx_bytes_processed;
  uint8_t payload_length = bytes_remaining < LIFI_PACKAGE_MAX_PAYLOAD_SIZE
                               ? bytes_remaining
                               : LIFI_PACKAGE_MAX_PAYLOAD_SIZE;

  LiFi_Protocol_TransmitPackage(socket, PACKAGE_TYPE_PAYLOAD, socket->tx_package_id,
                                socket->tx_buffer + socket->tx_bytes_processed, payload_length);
  socket->state = LIFI_SOCKET_TRANSMITTING;
}

void LiFi_Protocol_SendNextPayload(LiFi_Socket_t *socket) {
  ++socket->tx_package_id;
  send_payload(socket);
}

void LiFi_Protocol_SendStatus(LiFi_Socket_t *socket) {
  LiFi_Protocol_SendControl(socket, PACKAGE_TYPE_STATUS, socket->tx_package_id);
}

static void send_eot(LiFi_Socket_t *socket) {
  LiFi_Protocol_TransmitPackage(socket, PACKAGE_TYPE_EOT, socket->tx_package_id, NULL, 0);
  socket->state = LIFI_SOCKET_TRANSMITTING;
}

void LiFi_Protocol_RetryCurrentPayloadOrFail(LiFi_Socket_t *socket) {
  ++socket->tx_retries_count;

  if (socket->tx_retries_count >= MAX_TRANSMIT_RETRIES_COUNT) {
    LiFi_Protocol_NotifyErrorAndReset(socket);
    return;
  }

  send_payload(socket);
}

void LiFi_Protocol_RetryEotOrFail(LiFi_Socket_t *socket) {
  ++socket->tx_retries_count;

  if (socket->tx_retries_count >= MAX_TRANSMIT_RETRIES_COUNT) {
    LiFi_Protocol_NotifyErrorAndReset(socket);
    return;
  }

  send_eot(socket);
}

void LiFi_Protocol_RetryStatusOrFail(LiFi_Socket_t *socket) {
  ++socket->tx_retries_count;

  if (socket->tx_retries_count >= MAX_TRANSMIT_RETRIES_COUNT) {
    LiFi_Protocol_NotifyErrorAndReset(socket);
    return;
  }

  LiFi_Protocol_SendStatus(socket);
}

void LiFi_Protocol_HandleConfirmationPayload(LiFi_Socket_t *socket) {
  PackageType_t type = (PackageType_t)socket->rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX];

  if (type == PACKAGE_TYPE_BUSY) {
    socket->tx_retries_count = 0;
    socket->state = LIFI_SOCKET_WAITING_READY;
    return;
  }

  if (type == PACKAGE_TYPE_ACK_BUSY) {
    socket->tx_retries_count = 0;
    socket->tx_bytes_processed += socket->tx_package[TX_PACKAGE_LENGTH_INDEX];
    socket->state = LIFI_SOCKET_WAITING_READY;
    return;
  }

  if (type != PACKAGE_TYPE_ACK_READY) {
    LiFi_Protocol_RetryCurrentPayloadOrFail(socket);
    return;
  }

  // got ACK_READY for PAYLOAD, continue transmission
  socket->tx_retries_count = 0;
  socket->tx_bytes_processed += socket->tx_package[TX_PACKAGE_LENGTH_INDEX];

  if (socket->tx_bytes_processed < socket->tx_buffer_length) {
    LiFi_Protocol_SendNextPayload(socket);
    return;
  }

  send_eot(socket);
}

void LiFi_Protocol_HandleConfirmationEot(LiFi_Socket_t *socket) {
  PackageType_t type = (PackageType_t)socket->rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX];

  if (type == PACKAGE_TYPE_ACK_READY || type == PACKAGE_TYPE_ACK_BUSY) {
    LiFi_Protocol_ClearTransaction(socket);

    if (socket->on_transmission_success_callback != NULL &&
        socket->on_transmission_success_callback_context != NULL) {
      socket->on_transmission_success_callback(socket->on_transmission_success_callback_context);
    }

    return;
  }

  if (type == PACKAGE_TYPE_BUSY) {
    socket->tx_retries_count = 0;
    socket->state = LIFI_SOCKET_WAITING_READY;
    return;
  }

  LiFi_Protocol_RetryEotOrFail(socket);
}

void LiFi_Protocol_HandleWaitingReadyResponse(LiFi_Socket_t *socket) {
  PackageType_t type = (PackageType_t)socket->rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX];

  if (type == PACKAGE_TYPE_ACK_BUSY) {
    socket->tx_retries_count = 0;
    socket->state = LIFI_SOCKET_WAITING_READY;
    return;
  }

  if (type != PACKAGE_TYPE_READY) {
    // NAK: receiver rejected the STATUS itself - a failed attempt, not proof it's fine to wait.
    LiFi_Protocol_RetryStatusOrFail(socket);
    return;
  }

  socket->tx_retries_count = 0;

  if (socket->tx_bytes_processed < socket->tx_buffer_length) {
    LiFi_Protocol_SendNextPayload(socket);
    return;
  }

  send_eot(socket);

  if (socket->on_transmission_success_callback != NULL &&
      socket->on_transmission_success_callback_context != NULL) {
    socket->on_transmission_success_callback(socket->on_transmission_success_callback_context);
  }
}

void LiFi_Protocol_OnBufferTransmitted(void *context) {
  LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

  switch (socket->state) {
  case LIFI_SOCKET_TRANSMITTING:
    socket->state = LIFI_SOCKET_WAITING_CONFIRMATION;
    socket->rx_package_bytes_received = 0;
    LiFi_Transmitter_ToConfirmationMode(socket->transmitter);
    break;

  case LIFI_SOCKET_SENDING_CONTROL:
    if (socket->tx_package[TX_PACKAGE_PACKAGE_TYPE_INDEX] == PACKAGE_TYPE_STATUS) {
      socket->state = LIFI_SOCKET_WAITING_READY;
    } else if (socket->tx_package[TX_PACKAGE_PACKAGE_TYPE_INDEX] == PACKAGE_TYPE_ACK_BUSY) {
      socket->state = LIFI_SOCKET_RX_PAUSED;
    } else {
      socket->state = LIFI_SOCKET_RECEIVING;
    }
    break;

  case LIFI_SOCKET_SENDING_CONTROL_BUSY:
    socket->state = LIFI_SOCKET_RX_PAUSED;
    break;

  case LIFI_SOCKET_ACK_EOD:
    socket->state = LIFI_SOCKET_IDLE;

  default:
    break;
  }
}

void LiFi_Protocol_OnTransmitterTimeout(void *context) {
  LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

  if (socket->state == LIFI_SOCKET_WAITING_CONFIRMATION &&
      socket->tx_package[TX_PACKAGE_PACKAGE_TYPE_INDEX] == PACKAGE_TYPE_EOT) {
    LiFi_Protocol_RetryEotOrFail(socket);
  } else if (socket->state == LIFI_SOCKET_WAITING_CONFIRMATION) {
    LiFi_Protocol_RetryCurrentPayloadOrFail(socket);
  } else if (socket->state == LIFI_SOCKET_WAITING_READY) {
    // Only a genuinely unanswered STATUS counts against the limit - a receiver that keeps
    // replying ACK_BUSY (still legitimately busy) must never be capped out by this.
    LiFi_Protocol_RetryStatusOrFail(socket);
  }
}
