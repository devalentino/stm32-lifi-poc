#ifndef LIFI_MODEM_H
#define LIFI_MODEM_H

#include "lifi_host_interface.h"
#include "lifi_protocol.h"

typedef struct {
  LiFi_HostInterface_t *host_interface;
  LiFi_Socket_t *socket;
} LiFi_Modem_t;

void LiFi_Modem_Init(LiFi_Modem_t *modem, LiFi_HostInterface_t *host_interface,
                     LiFi_Socket_t *socket);

void LiFi_Modem_onLiFiTransmittionSuccessfullCallback(void *context);

void LiFi_Model_onLiFiTransmissionErrorCallback(void *context);

#endif