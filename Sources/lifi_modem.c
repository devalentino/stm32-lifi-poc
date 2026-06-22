#include "lifi_modem.h"

static void on_lifi_transmission_successful(void *context) {
  LiFi_Modem_t *modem = (LiFi_Modem_t *)context;
  LiFi_HostInterface_ConsumeData(modem->host_interface);
}

static void on_lifi_transmission_error(LiFi_Socket_Error_t error, void *context) {
  (void)error;
  LiFi_Modem_t *modem = (LiFi_Modem_t *)context;
  LiFi_HostInterface_ConsumeData(modem->host_interface);
}

static void on_host_interface_buffer_received(void *context) {
  LiFi_Modem_t *modem = (LiFi_Modem_t *)context;

  LiFi_Socket_Send(modem->socket,
                   modem->host_interface->rx_buffer + modem->host_interface->rx_buffer_shift,
                   modem->host_interface->rx_buffer_length);
}

void LiFi_Modem_Init(LiFi_Modem_t *modem, LiFi_HostInterface_t *host_interface,
                     LiFi_Socket_t *socket) {
  modem->host_interface = host_interface;
  modem->socket = socket;

  host_interface->on_buffer_received_callback = on_host_interface_buffer_received;
  host_interface->on_buffer_received_callback_context = modem;

  socket->on_transmission_success_callback = on_lifi_transmission_successful;
  socket->on_transmission_success_callback_context = modem;
  socket->on_error_callback = on_lifi_transmission_error;
  socket->on_error_callback_context = modem;
}
