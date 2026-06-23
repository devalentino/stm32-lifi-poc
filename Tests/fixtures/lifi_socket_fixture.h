#ifndef LIFI_SOCKET_FIXTURE_H
#define LIFI_SOCKET_FIXTURE_H

#include "lifi_protocol.h"

typedef struct {
  LiFi_Transmitter_t transmitter;
  LiFi_Receiver_t receiver;
  LiFi_Socket_t socket;
} LiFi_Socket_Fixture_t;

typedef struct {
  LiFi_Socket_Fixture_t sender;
  LiFi_Socket_Fixture_t recipient;
} LiFi_Socket_Pair_Fixture_t;

void LiFi_Socket_Fixture_Init(LiFi_Socket_Fixture_t *fixture);
void LiFi_Socket_Pair_Fixture_Init(LiFi_Socket_Pair_Fixture_t *fixture);

#endif
