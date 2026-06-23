#include <stdint.h>
#include <string.h>

#include "fake_lifi_callbacks.h"
#include "fake_lifi_transport.h"
#include "lifi_modem_fixture.h"
#include "test_suites.h"
#include "unity.h"

static void test_lifi_modem__host_payload_is_sent_through_socket(void) {
  LiFi_Modem_Fixture_t fixture;
  LiFi_Modem_Fixture_Init(&fixture);
  const uint8_t payload[] = {'H', 'a', 'l', 'l', 'o'};
  memcpy(fixture.modem.host_interface->rx_buffer, payload, sizeof(payload));

  LiFi_HostInterface_onUartDataReceivedCallback(fixture.modem.host_interface, sizeof(payload), 0);

  TEST_ASSERT_EQUAL_UINT8(sizeof(payload), fixture.modem.socket->tx_buffer_length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, fixture.modem.socket->tx_buffer, sizeof(payload));
}

static void test_lifi_modem__successful_socket_transmission_releases_host(void) {
  LiFi_Modem_Fixture_t fixture;
  LiFi_Modem_Fixture_Init(&fixture);
  const uint8_t payload[] = {'H', 'a', 'l', 'l', 'o'};
  uint8_t peer_rx_buffer[sizeof(payload)] = {0};
  memcpy(fixture.modem.host_interface->rx_buffer, payload, sizeof(payload));
  LiFi_Socket_Read(&fixture.peer.socket, peer_rx_buffer);

  LiFi_HostInterface_onUartDataReceivedCallback(fixture.modem.host_interface, sizeof(payload), 0);
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_HEX8(0x11, fixture.modem.host_interface->huart->Instance->DR);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, fixture.peer.socket.rx_buffer, sizeof(payload));
}

static void test_lifi_modem__socket_nak_releases_host(void) {
  LiFi_Modem_Fixture_t fixture;
  LiFi_Modem_Fixture_Init(&fixture);
  const uint8_t payload[] = {'H', 'a', 'l', 'l', 'o'};
  uint8_t peer_rx_buffer[sizeof(payload)] = {0};
  memcpy(fixture.modem.host_interface->rx_buffer, payload, sizeof(payload));
  LiFi_Socket_Read(&fixture.peer.socket, peer_rx_buffer);

  LiFi_HostInterface_onUartDataReceivedCallback(fixture.modem.host_interface, sizeof(payload), 0);

  // Corrupt the CRC byte after the payload so the peer rejects the packet and sends NAK.
  fixture.modem.socket->transmitter->tx_buffer[TX_PACKAGE_HEADER_BYTES + sizeof(payload)] ^= 0xFF;
  fixture.modem.socket->tx_retries_count = MAX_TRANSMIT_RETRIES_COUNT - 1;
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_HEX8(0x11, fixture.modem.host_interface->huart->Instance->DR);
  TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t[sizeof(payload)]){0}, fixture.peer.socket.rx_buffer,
                                sizeof(payload));
}

void Test_LiFi_Modem_Run(void) {
  RUN_TEST(test_lifi_modem__host_payload_is_sent_through_socket);
  RUN_TEST(test_lifi_modem__successful_socket_transmission_releases_host);
  RUN_TEST(test_lifi_modem__socket_nak_releases_host);
}
