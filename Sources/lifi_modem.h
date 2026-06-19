#ifndef LIFI_MODEM_H
#define LIFI_MODEM_H

#include "lifi_protocol.h";
#include "lifi_socket.h";
#include "lifi_host_interface.h";

typedef struct {
  LiFi_HostInterface_t *host_interface;
  LiFi_Socket_t *socket;
} LiFi_Modem_t;

void LiFi_Modem_Init(LiFi_Modem_t *modem, LiFi_HostInterface_t *host_interface, LiFi_Socket_t *socket);

#endif