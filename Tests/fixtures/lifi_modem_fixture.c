#include "lifi_modem_fixture.h"

#include <string.h>

#include "fake_lifi_callbacks.h"
#include "fake_lifi_transport.h"

void LiFi_Modem_Fixture_Init(LiFi_Modem_Fixture_t *fixture) {
  memset(fixture, 0, sizeof(*fixture));

  Fake_LiFi_Link_Reset();
  LiFi_Transmitter_TransmitBuffer_fake.custom_fake =
      Fake_LiFi_Transmitter_TransmitBuffer_Callback;
  LiFi_Socket_Fixture_Init(&fixture->modem_socket_storage);
  LiFi_Socket_Fixture_Init(&fixture->peer);
  Fake_LiFi_Link_Register(&fixture->modem_socket_storage.transmitter,
                          &fixture->peer.receiver);
  Fake_LiFi_Link_Register(&fixture->peer.transmitter,
                          &fixture->modem_socket_storage.receiver);

  fixture->huart.Instance = &fixture->usart;
  LiFi_HostInterface_Init(&fixture->host_interface, &fixture->huart, fixture->host_tx_storage,
                          fixture->host_rx_storage);
  LiFi_Modem_Init(&fixture->modem, &fixture->host_interface,
                  &fixture->modem_socket_storage.socket);
}
