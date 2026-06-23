#include "lifi_modem_fixture.h"

#include <string.h>

#include "fake_lifi_callbacks.h"

void LiFi_Modem_Fixture_Init(LiFi_Modem_Fixture_t *fixture) {
  memset(fixture, 0, sizeof(*fixture));

  /* Protocol tests install a transport implementation in the shared fake. */
  LiFi_Transmitter_TransmitBuffer_fake.custom_fake = NULL;
  LiFi_Socket_Fixture_Init(&fixture->socket);

  fixture->huart.Instance = &fixture->usart;
  LiFi_HostInterface_Init(&fixture->host_interface, &fixture->huart, fixture->host_tx_buffer,
                          fixture->host_rx_buffer);
  LiFi_Modem_Init(&fixture->modem, &fixture->host_interface, &fixture->socket.socket);
}
