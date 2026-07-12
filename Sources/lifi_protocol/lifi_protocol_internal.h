#ifndef LIFI_PROTOCOL_INTERNAL_H
#define LIFI_PROTOCOL_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lifi_protocol.h"

// lifi_protocol.c - socket lifecycle
void LiFi_Protocol_ClearTransaction(LiFi_Socket_t *socket);
void LiFi_Protocol_NotifyErrorAndReset(LiFi_Socket_t *socket);

// lifi_protocol_utils.c - package framing/CRC helpers
void LiFi_Protocol_BuildPackage(uint8_t *destination, PackageType_t type, uint8_t id,
                                const uint8_t *payload, uint8_t payload_length);
bool LiFi_Protocol_ReceivedPackageCrcIsValid(const LiFi_Socket_t *socket);

// lifi_protocol_tx.c - sender state machine
void LiFi_Protocol_TransmitPackage(LiFi_Socket_t *socket, PackageType_t type, uint8_t id,
                                   const uint8_t *payload, uint8_t payload_length);
void LiFi_Protocol_SendControl(LiFi_Socket_t *socket, PackageType_t type, uint8_t id);
void LiFi_Protocol_SendNextPayload(LiFi_Socket_t *socket);
void LiFi_Protocol_SendStatus(LiFi_Socket_t *socket);
void LiFi_Protocol_RetryCurrentPayloadOrFail(LiFi_Socket_t *socket);
void LiFi_Protocol_RetryEotOrFail(LiFi_Socket_t *socket);
void LiFi_Protocol_RetryStatusOrFail(LiFi_Socket_t *socket);
void LiFi_Protocol_HandleConfirmationPayload(LiFi_Socket_t *socket);
void LiFi_Protocol_HandleConfirmationEot(LiFi_Socket_t *socket);
void LiFi_Protocol_HandleWaitingReadyResponse(LiFi_Socket_t *socket);
void LiFi_Protocol_OnBufferTransmitted(void *context);
void LiFi_Protocol_OnTransmitterTimeout(void *context);

// lifi_protocol_rx.c - receiver state machine
void LiFi_Protocol_SendAck(LiFi_Socket_t *socket);
void LiFi_Protocol_SendNak(LiFi_Socket_t *socket, uint8_t package_id);
void LiFi_Protocol_SendBusy(LiFi_Socket_t *socket, uint8_t package_id);
void LiFi_Protocol_SendEotAck(LiFi_Socket_t *socket);
void LiFi_Protocol_HandlePayload(LiFi_Socket_t *socket);
void LiFi_Protocol_SendReady(LiFi_Socket_t *socket);
void LiFi_Protocol_SendAckBusy(LiFi_Socket_t *socket);
void LiFi_Protocol_HandleEot(LiFi_Socket_t *socket);
void LiFi_Protocol_OnReceiverTimeout(void *context);
void LiFi_Protocol_OnByteReceived(void *context);

// lifi_protocol_router.c - received package dispatch
void LiFi_Protocol_HandleReceivedPackage(LiFi_Socket_t *socket);

#endif
