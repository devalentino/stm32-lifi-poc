#include "lifi_protocol_internal.h"

void LiFi_Protocol_HandleReceivedPackage(LiFi_Socket_t *socket) {
  bool crc_is_valid = LiFi_Protocol_ReceivedPackageCrcIsValid(socket);
  PackageType_t type = (PackageType_t)socket->rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX];
  uint8_t rx_id = socket->rx_package[RX_PACKAGE_ID_INDEX];

  switch (socket->state) {
  case LIFI_SOCKET_WAITING_CONFIRMATION:
    // Transmitter is waiting for confirmation and got unexpected type
    if (!crc_is_valid || rx_id != socket->tx_package_id ||
        (type != PACKAGE_TYPE_ACK_READY && type != PACKAGE_TYPE_ACK_BUSY &&
         type != PACKAGE_TYPE_NAK && type != PACKAGE_TYPE_BUSY)) {
      if (socket->tx_package[TX_PACKAGE_PACKAGE_TYPE_INDEX] == PACKAGE_TYPE_EOT) {
        LiFi_Protocol_RetryEotOrFail(socket);
      } else {
        LiFi_Protocol_RetryCurrentPayloadOrFail(socket);
      }
      return;
    }

    // Transmitter is waiting for confirmation and got expected type
    if (socket->tx_package[TX_PACKAGE_PACKAGE_TYPE_INDEX] == PACKAGE_TYPE_EOT) {
      LiFi_Protocol_HandleConfirmationEot(socket);
    } else {
      LiFi_Protocol_HandleConfirmationPayload(socket);
    }
    return;

  case LIFI_SOCKET_WAITING_READY:
    // Transmitter is waiting for receiver to get ready, sent status and got bad package
    if (!crc_is_valid || rx_id != socket->tx_package_id ||
        (type != PACKAGE_TYPE_READY && type != PACKAGE_TYPE_ACK_BUSY && type != PACKAGE_TYPE_NAK)) {
      LiFi_Protocol_RetryStatusOrFail(socket);
      return;
    }

    // Transmitter is waiting for receiver to get ready, sent status and handles response
    LiFi_Protocol_HandleWaitingReadyResponse(socket);
    return;

  case LIFI_SOCKET_RECEIVING:
    // Receiver got already processed package
    if (crc_is_valid && type == PACKAGE_TYPE_PAYLOAD && rx_id == socket->rx_package_id) {
      LiFi_Protocol_SendAck(socket);
    // Receiver got new payload (positive scenario)
    } else if (crc_is_valid && type == PACKAGE_TYPE_PAYLOAD &&
               rx_id == (uint8_t)(socket->rx_package_id + 1)) {
      LiFi_Protocol_HandlePayload(socket);
    // Receiver got EOT
    } else if (crc_is_valid && type == PACKAGE_TYPE_EOT && rx_id == socket->rx_package_id) {
      LiFi_Protocol_HandleEot(socket);
    // Receiver got status request
    } else if (crc_is_valid && type == PACKAGE_TYPE_STATUS && rx_id == socket->rx_package_id) {
      LiFi_Protocol_SendReady(socket);
    } else {
      LiFi_Protocol_SendNak(socket, rx_id);
    }
    return;

  case LIFI_SOCKET_RX_PAUSED:
    if (crc_is_valid && type == PACKAGE_TYPE_STATUS && rx_id == socket->rx_package_id) {
      LiFi_Protocol_SendAckBusy(socket);
    } else {
      LiFi_Protocol_SendBusy(socket, rx_id);
    }
    return;

  case LIFI_SOCKET_IDLE:
    // Receiver already finished receiving data and got idle, but transmitter retried EOD
    if (crc_is_valid && type == PACKAGE_TYPE_EOT && rx_id == socket->rx_package_id) {
      LiFi_Protocol_SendEotAck(socket);
    }
    // Anything else while idle (garbled bytes, a stale/foreign EOT) is not part of an
    // in-progress transaction, so there is no peer to notify - silently drop it.
    return;

  default:
    return;
  }
}
