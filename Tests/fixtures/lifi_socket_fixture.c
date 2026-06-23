#include "lifi_socket_fixture.h"

#include <string.h>

#include "fake_lifi_callbacks.h"

void LiFi_Socket_Fixture_Init(LiFi_Socket_Fixture_t *fixture) {
  memset(fixture, 0, sizeof(*fixture));

  LiFi_Socket_Init(
      &fixture->socket, &fixture->transmitter, &fixture->receiver,
      Mock_LiFi_Socket_onErrorCallback, &fixture->socket,
      Mock_LiFi_Socket_onTransmissionSuccessfulCallback, &fixture->socket,
      Mock_LiFi_Socket_onReceiveSuccessfulCallback, &fixture->socket);
}
