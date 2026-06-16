#ifndef LIFI_PROTOCOL_H
#define LIFI_PROTOCOL_H

#include <stdint.h>

#include "lifi_receiver.h"
#include "lifi_transmitter.h"

#define PREAMBULE 0x55
#define MAX_TRANSMIT_RETRIES_COUNT 0x5

#define TX_PACKAGE_PREMBULE_INDEX 0
#define TX_PACKAGE_START_INDEX 1
#define TX_PACKAGE_PACKAGE_TYPE_INDEX 2
#define TX_PACKAGE_ID_INDEX 3
#define TX_PACKAGE_LENGTH_INDEX 4
#define TX_PACKAGE_HEADER_BYTES 5

#define RX_PACKAGE_START_INDEX 0
#define RX_PACKAGE_PACKAGE_TYPE_INDEX 1
#define RX_PACKAGE_ID_INDEX 2
#define RX_PACKAGE_LENGTH_INDEX 3
#define RX_PACKAGE_HEADER_BYTES 4

typedef enum {
  PACKAGE_TYPE_ACK = 0x56,
  PACKAGE_TYPE_NAK = 0x57,
  PACKAGE_TYPE_PAYLOAD = 0x58,
  PACKAGE_TYPE_EOT = 0x59
} PackageType_t;

typedef enum { LIFI_SOCKET_CONNECTION_ERROR } LiFi_Socket_Error_t;

typedef struct LiFi_Socket_t LiFi_Socket_t;

typedef void (*LiFi_Socket_onErrorCallback)(LiFi_Socket_Error_t error, LiFi_Socket_t *socket);
typedef void (*LiFi_Socket_onTransmissionSuccessfulCallback)(LiFi_Socket_t *socket);
typedef void (*LiFi_Socket_onReceiveSuccessfulCallback)(LiFi_Socket_t *socket);

struct LiFi_Socket_t {
  LiFi_Transmitter_t *transmitter;
  LiFi_Receiver_t *receiver;

  bool is_busy;
  LiFi_Socket_onErrorCallback on_error_callback;
  LiFi_Socket_onTransmissionSuccessfulCallback on_transmission_success_callback;
  LiFi_Socket_onReceiveSuccessfulCallback on_receive_success_callback;

  uint8_t *tx_buffer;
  uint8_t tx_buffer_length;
  uint8_t tx_bytes_processed;
  uint8_t tx_package[LIFI_TX_BUFFER_SIZE];
  uint8_t tx_package_id;
  uint8_t tx_package_type;
  uint8_t tx_retries_count;
  bool is_tx_confirmation_required;

  uint8_t *rx_buffer;
  uint8_t rx_package[LIFI_TX_BUFFER_SIZE];
  uint8_t rx_package_id;
  uint8_t rx_package_bytes_received;
};

void LiFi_Socket_Init(LiFi_Socket_t *socket, LiFi_Transmitter_t *transmitter,
                      LiFi_Receiver_t *receiver, LiFi_Socket_onErrorCallback on_error_callback,
                      LiFi_Socket_onTransmissionSuccessfulCallback on_transmission_success_callback,
                      LiFi_Socket_onReceiveSuccessfulCallback on_receive_success_callback);

bool LiFi_Socket_Send(LiFi_Socket_t *socket, uint8_t *buffer, uint8_t length);

bool LiFi_Socket_Read(LiFi_Socket_t *socket, uint8_t *buffer);

// TODO: next functions can be static functions, not part of Public API
void LiFi_Socket_Ack(LiFi_Socket_t *socket, uint8_t package_id);

void LiFi_Socket_Nak(LiFi_Socket_t *socket, uint8_t package_id);

void LiFi_Socket_EOT(LiFi_Socket_t *socket, uint8_t package_id);

#endif
