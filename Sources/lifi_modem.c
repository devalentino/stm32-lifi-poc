#include "lifi_modem.h"

static void on_host_interface_buffer_received(void *context) {
  LiFi_Modem_t *modem = (LiFi_Modem_t *)context;

  // TODO: send XON, call modem to transmit buffer through LiFi
  (void)modem;
}

void LiFi_Modem_Init(LiFi_Modem_t *modem, LiFi_HostInterface_t *host_interface,
                     LiFi_Socket_t *socket) {
  modem->host_interface = host_interface;
  modem->socket = socket;

  host_interface->on_buffer_received_callback = on_host_interface_buffer_received;
  host_interface->on_buffer_received_callback_context = modem;
}
