#include "test_suites.h"
#include "lifi_modem.h"
#include "lifi_"
#include "unity.h"
#include <stdint.h>

static void test_lifi_modem__host_sent_payload(void) {
  uint8_t tx_buffer[128] = {0};
  uint8_t rx_buffer[128] = {0};

  LiFi_HostInterface_t host_interface = {0};
  LiFi_HostInterface_Init(host_interface, NULL, tx_buffer, rx_buffer);
  
  LiFi_Modem_t modem = {0};
  LiFi_Modem_Init(modem, host_interface);

  LiFi_HostInterface_onReceiveSuccessfulCallback();

  // TODO: assert XON is sent back, LiFi_Socket started data processing
}

void Test_LiFi_Modem_Run(void) {
  RUN_TEST(test_lifi_modem__host_sent_payload);
}
