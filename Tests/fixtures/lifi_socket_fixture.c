#include "lifi_socket_fixture.h"

#include <string.h>

#include "fake_lifi_callbacks.h"
#include "fake_lifi_transport.h"

void LiFi_Socket_Fixture_Init(LiFi_Socket_Fixture_t *fixture) {
  memset(fixture, 0, sizeof(*fixture));

  LiFi_Socket_Init(&fixture->socket, &fixture->transmitter, &fixture->receiver,
                   Mock_LiFi_Socket_onErrorCallback, &fixture->socket,
                   Mock_LiFi_Socket_onTransmissionSuccessfulCallback, &fixture->socket,
                   Mock_LiFi_Socket_onReceiveSuccessfulCallback, &fixture->socket);
}

void LiFi_Socket_Pair_Fixture_Init(LiFi_Socket_Pair_Fixture_t *fixture) {
  LiFi_Socket_Fixture_Init(&fixture->sender);
  LiFi_Socket_Fixture_Init(&fixture->recipient);
  fixture->sender.socket.name = "sender";
  fixture->recipient.socket.name = "recipient";

  Fake_LiFi_Link_Register(fixture->sender.socket.transmitter, fixture->recipient.socket.receiver);
  Fake_LiFi_Link_Register(fixture->recipient.socket.transmitter, fixture->sender.socket.receiver);
}
