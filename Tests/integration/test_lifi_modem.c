#include <stdint.h>
#include <string.h>

#include "fake_lifi_callbacks.h"
#include "lifi_modem_fixture.h"
#include "test_suites.h"
#include "unity.h"

static void test_lifi_modem__host_payload_is_sent_through_socket(void) {
  LiFi_Modem_Fixture_t fixture;
  LiFi_Modem_Fixture_Init(&fixture);
  const uint8_t payload[] = {'H', 'a', 'l', 'l', 'o'};
  memcpy(fixture.host_rx_buffer, payload, sizeof(payload));

  LiFi_HostInterface_onUartDataReceivedCallback(&fixture.host_interface, sizeof(payload), 0);

  TEST_ASSERT_EQUAL_PTR(fixture.host_rx_buffer, fixture.socket.socket.tx_buffer);
  TEST_ASSERT_EQUAL_UINT8(sizeof(payload), fixture.socket.socket.tx_buffer_length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, fixture.socket.socket.tx_buffer, sizeof(payload));
}

static void test_lifi_modem__successful_transmission_releases_host(void) {
  LiFi_Modem_Fixture_t fixture;
  LiFi_Modem_Fixture_Init(&fixture);

  fixture.socket.socket.on_transmission_success_callback(
      fixture.socket.socket.on_transmission_success_callback_context);

  TEST_ASSERT_EQUAL_HEX8(0x11, fixture.usart.DR);
}

static void test_lifi_modem__transmission_error_releases_host(void) {
  LiFi_Modem_Fixture_t fixture;
  LiFi_Modem_Fixture_Init(&fixture);

  fixture.socket.socket.on_error_callback(
      LIFI_SOCKET_CONNECTION_ERROR, fixture.socket.socket.on_error_callback_context);

  TEST_ASSERT_EQUAL_HEX8(0x11, fixture.usart.DR);
}

static void test_lifi_modem__does_not_replace_receive_callback(void) {
  LiFi_Modem_Fixture_t fixture;
  LiFi_Modem_Fixture_Init(&fixture);

  fixture.socket.socket.on_receive_success_callback(
      fixture.socket.socket.on_receive_success_callback_context);

  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onReceiveSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&fixture.socket.socket,
                        Mock_LiFi_Socket_onReceiveSuccessfulCallback_fake.arg0_val);
}

void Test_LiFi_Modem_Run(void) {
  RUN_TEST(test_lifi_modem__host_payload_is_sent_through_socket);
  RUN_TEST(test_lifi_modem__successful_transmission_releases_host);
  RUN_TEST(test_lifi_modem__transmission_error_releases_host);
  RUN_TEST(test_lifi_modem__does_not_replace_receive_callback);
}
