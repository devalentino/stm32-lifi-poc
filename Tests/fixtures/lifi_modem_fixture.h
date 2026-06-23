#ifndef LIFI_MODEM_FIXTURE_H
#define LIFI_MODEM_FIXTURE_H

#include <stdint.h>

#include "lifi_modem.h"
#include "lifi_socket_fixture.h"

#define LIFI_MODEM_FIXTURE_HOST_BUFFER_SIZE 128

typedef struct {
  LiFi_Socket_Fixture_t modem_socket_storage;
  LiFi_Socket_Fixture_t peer;

  uint8_t host_tx_storage[LIFI_MODEM_FIXTURE_HOST_BUFFER_SIZE];
  uint8_t host_rx_storage[LIFI_MODEM_FIXTURE_HOST_BUFFER_SIZE];
  USART_TypeDef usart;
  UART_HandleTypeDef huart;
  LiFi_HostInterface_t host_interface;

  LiFi_Modem_t modem;
} LiFi_Modem_Fixture_t;

void LiFi_Modem_Fixture_Init(LiFi_Modem_Fixture_t *fixture);

#endif
