#include "lifi_protocol_internal.h"

#include <string.h>

void LiFi_Protocol_SendAck(LiFi_Socket_t *socket) {
  LiFi_Protocol_SendControl(socket, PACKAGE_TYPE_ACK_READY, socket->rx_package_id);
}

void LiFi_Protocol_SendNak(LiFi_Socket_t *socket, uint8_t package_id) {
  LiFi_Protocol_SendControl(socket, PACKAGE_TYPE_NAK, package_id);
}

void LiFi_Protocol_SendBusy(LiFi_Socket_t *socket, uint8_t package_id) {
  LiFi_Protocol_SendControl(socket, PACKAGE_TYPE_BUSY, package_id);
}

void LiFi_Protocol_SendEotAck(LiFi_Socket_t *socket) {
  LiFi_Protocol_SendControl(socket, PACKAGE_TYPE_ACK_READY, socket->rx_package_id);
  socket->state = LIFI_SOCKET_ACK_EOD;
}

void LiFi_Protocol_SendReady(LiFi_Socket_t *socket) {
  LiFi_Protocol_SendControl(socket, PACKAGE_TYPE_READY, socket->rx_package_id);
}

void LiFi_Protocol_SendAckBusy(LiFi_Socket_t *socket) {
  LiFi_Protocol_SendControl(socket, PACKAGE_TYPE_ACK_BUSY, socket->rx_package_id);
}

void LiFi_Protocol_HandlePayload(LiFi_Socket_t *socket) {
  uint8_t payload_length = socket->rx_package[RX_PACKAGE_LENGTH_INDEX];
  memcpy(socket->rx_buffer + socket->rx_bytes_received,
         socket->rx_package + RX_PACKAGE_HEADER_BYTES, payload_length);
  socket->rx_bytes_received += payload_length;
  socket->rx_package_id = socket->rx_package[RX_PACKAGE_ID_INDEX];
  LiFi_Protocol_SendAck(socket);
}

void LiFi_Protocol_HandleEot(LiFi_Socket_t *socket) {
  LiFi_Protocol_SendEotAck(socket);

  if (socket->on_receive_success_callback != NULL &&
      socket->on_receive_success_callback_context != NULL) {
    socket->on_receive_success_callback(socket->on_receive_success_callback_context);
  }
}

void LiFi_Protocol_OnReceiverTimeout(void *context) {
  LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

  if (socket->state == LIFI_SOCKET_RECEIVING || socket->state == LIFI_SOCKET_WAITING_CONFIRMATION ||
      socket->state == LIFI_SOCKET_SENDING_CONTROL) {
    LiFi_Protocol_NotifyErrorAndReset(socket);
  }
}

void LiFi_Protocol_OnByteReceived(void *context) {
  LiFi_Socket_t *socket = (LiFi_Socket_t *)context;

  if (socket->state != LIFI_SOCKET_RECEIVING && socket->state != LIFI_SOCKET_WAITING_CONFIRMATION &&
      socket->state != LIFI_SOCKET_WAITING_READY && socket->state != LIFI_SOCKET_RX_PAUSED &&
      socket->state != LIFI_SOCKET_IDLE) {
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

  LiFi_Protocol_HandleReceivedPackage(socket);
  socket->rx_package_bytes_received = 0;
}
