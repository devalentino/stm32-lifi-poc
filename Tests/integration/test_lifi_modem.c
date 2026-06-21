#include "lifi_modem.h"
#include "test_suites.h"
#include "unity.h"
#include <stdint.h>

static void test_lifi_modem__host_sent_payload(void) {
  LiFi_Socket_t socket = {0};

  uint8_t tx_buffer[128] = {0};
  uint8_t rx_buffer[128] = {'H', 'a', 'l', 'l', 'o'};

  LiFi_HostInterface_t host_interface = {0};
  LiFi_HostInterface_Init(&host_interface, NULL, tx_buffer, rx_buffer);

  LiFi_Modem_t modem = {0};
  LiFi_Modem_Init(&modem, &host_interface, &socket);

  LiFi_HostInterface_onUartDataReceivedCallback(&host_interface, sizeof(rx_buffer), 0);

  // TODO: assert XOFF is sent, LiFi_Socket started data processing, XON is sent on lifi transmission ended
}

void Test_LiFi_Modem_Run(void) {
  RUN_TEST(test_lifi_modem__host_sent_payload);
}
