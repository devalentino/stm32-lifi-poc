#include "lifi_protocol.h"

#include <string.h>

#include "lifi_protocol_internal.h"

void LiFi_Protocol_ClearTransaction(LiFi_Socket_t *socket) {
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

void LiFi_Protocol_NotifyErrorAndReset(LiFi_Socket_t *socket) {
  if (socket->on_error_callback != NULL) {
    socket->on_error_callback(LIFI_SOCKET_CONNECTION_ERROR, socket->on_error_callback_context);
  }

  LiFi_Protocol_ClearTransaction(socket);
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
  socket->transmitter->on_buffer_transmitted = LiFi_Protocol_OnBufferTransmitted;
  socket->transmitter->on_buffer_transmitted_callback_context = socket;
  socket->transmitter->on_timeout_callback = LiFi_Protocol_OnTransmitterTimeout;
  socket->transmitter->on_timeout_callback_context = socket;

  socket->receiver = receiver;
  socket->receiver->on_byte_received = LiFi_Protocol_OnByteReceived;
  socket->receiver->on_byte_received_callback_context = socket;
  socket->receiver->on_timeout_callback = LiFi_Protocol_OnReceiverTimeout;
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

  LiFi_Protocol_SendNextPayload(socket);
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
