#include <stdint.h>

#include "fake_lifi_callbacks.h"
#include "fake_lifi_transport.h"
#include "lifi_socket_fixture.h"
#include "test_suites.h"
#include "unity.h"

void setUp(void) {
  Fake_LiFi_Callbacks_Reset();
  Fake_LiFi_Link_Reset();
  LiFi_Transmitter_TransmitBuffer_fake.custom_fake = Fake_LiFi_Transmitter_TransmitBuffer_Callback;
}

void tearDown(void) {
}

void test_transmit_payload(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, fixture.recipient.socket.rx_buffer, sizeof(payload));

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&fixture.sender.socket,
                        Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.arg0_val);

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onReceiveSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&fixture.recipient.socket,
                        Mock_LiFi_Socket_onReceiveSuccessfulCallback_fake.arg0_val);
}

void test_transmit_payload__wrong_crc(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));

  // Corrupt the CRC byte after the payload so the recipient rejects the packet and sends NAK.
  fixture.sender.socket.transmitter->tx_buffer[TX_PACKAGE_HEADER_BYTES + sizeof(payload)] ^= 0xFF;
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8(
      PACKAGE_TYPE_NAK,
      fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0,
                          fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_LENGTH_INDEX]);

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_NOT_EQUAL(LIFI_SOCKET_WAITING_CONFIRMATION, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_NAK,
                          fixture.sender.socket.rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.rx_package[RX_PACKAGE_LENGTH_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.recipient.socket.rx_package_bytes_received);
  TEST_ASSERT_EQUAL_UINT8(1, fixture.sender.socket.tx_retries_count);
}

void test_transmit_payload__socket_is_reset_after_retries_limit(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));

  // Corrupt the CRC and make the next NAK exhaust the retry limit.
  fixture.sender.socket.transmitter->tx_buffer[TX_PACKAGE_HEADER_BYTES + sizeof(payload)] ^= 0xFF;
  fixture.sender.socket.tx_retries_count = MAX_TRANSMIT_RETRIES_COUNT - 1;

  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_NOT_EQUAL(LIFI_SOCKET_WAITING_CONFIRMATION, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_NAK,
                          fixture.sender.socket.rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.rx_package[RX_PACKAGE_LENGTH_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.recipient.socket.rx_package_bytes_received);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_retries_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onErrorCallback_fake.call_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_CONNECTION_ERROR, Mock_LiFi_Socket_onErrorCallback_fake.arg0_val);
  TEST_ASSERT_EQUAL_PTR(&fixture.sender.socket, Mock_LiFi_Socket_onErrorCallback_fake.arg1_val);
}

void test_transmit_payload__receiver_ignores_package_on_wrong_start_byte(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));

  fixture.sender.socket.transmitter->tx_buffer[TX_PACKAGE_START_INDEX] = 0;
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8_ARRAY((uint8_t[sizeof(payload)]){0}, fixture.recipient.socket.rx_buffer,
                                sizeof(payload));
}

void test_socket_continue_transmission_after_confirmation(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {
      'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p', 's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's',
      'i', 't', ' ', 'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't', 'e', 't', 'u',
      'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n', 'g', ' ', 'e', 'l', 'i', 't', ',', ' ',
      's', 'e', 'd', ' ', 'd', 'o', ' ', 'e', 'i', 'u', 's', 'm', 'o', 'd', ' ', 't', 'e', 'm', 'p',
      'o', 'r', ' ', 'i', 'n', 'c', 'i', 'd', 'i', 'd', 'u', 'n', 't', ' ', 'u', 't'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  TEST_ASSERT_EQUAL_UINT8(1, fixture.sender.socket.tx_package_id);

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_CONFIRMATION, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT(1, LiFi_Transmitter_ToConfirmationMode_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(fixture.sender.socket.transmitter,
                        LiFi_Transmitter_ToConfirmationMode_fake.arg0_val);
  TEST_ASSERT_EQUAL_UINT8(
      PACKAGE_TYPE_ACK_READY,
      fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0,
                          fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_LENGTH_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(1, fixture.recipient.socket.rx_package_id);

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_NOT_EQUAL(LIFI_SOCKET_WAITING_CONFIRMATION, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_ACK_READY,
                          fixture.sender.socket.rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.rx_package[RX_PACKAGE_LENGTH_INDEX]);
  TEST_ASSERT_EQUAL_MEMORY(fixture.sender.socket.transmitter->tx_buffer + TX_PACKAGE_HEADER_BYTES,
                           fixture.sender.socket.tx_buffer +
                               fixture.sender.socket.tx_bytes_processed,
                           LIFI_TX_BUFFER_SIZE - TX_PACKAGE_HEADER_BYTES - 1);
  TEST_ASSERT_EQUAL_UINT8(2, fixture.sender.socket.tx_package_id);

  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, fixture.recipient.socket.rx_buffer, sizeof(payload));
  TEST_ASSERT_EQUAL_UINT8(sizeof(payload), fixture.recipient.socket.rx_bytes_received);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.recipient.socket.state);
}

void test_transmit_payload__confirmation_timeout(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, fixture.recipient.socket.rx_buffer, sizeof(payload));
  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_CONFIRMATION, fixture.sender.socket.state);

  fixture.sender.socket.tx_retries_count = MAX_TRANSMIT_RETRIES_COUNT - 1;
  LiFi_Transmitter_TimerCallback(fixture.sender.socket.transmitter);

  TEST_ASSERT_NULL(fixture.sender.socket.tx_buffer);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_buffer_length);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_bytes_processed);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_PTR(&fixture.sender.socket, Mock_LiFi_Socket_onErrorCallback_fake.arg1_val);
}

void test_receive_payload__connection_timeout(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, fixture.recipient.socket.rx_buffer, sizeof(payload));

  LiFi_Receiver_TimerCallback(fixture.recipient.socket.receiver);

  TEST_ASSERT_NULL(fixture.recipient.socket.rx_buffer);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.recipient.socket.rx_package_bytes_received);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.recipient.socket.state);
}

void test_transmit_payload__cant_transmit_to_the_busy_socket(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t another_payload[] = {'W', 'a', 't', '?'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));

  TEST_ASSERT_FALSE(
      LiFi_Socket_Send(&fixture.sender.socket, another_payload, sizeof(another_payload)));
}

void test_receive_payload__cant_start_another_read_while_receiving(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  TEST_ASSERT_TRUE(LiFi_Socket_Read(&fixture.recipient.socket, read_buffer));
  TEST_ASSERT_FALSE(LiFi_Socket_Read(&fixture.recipient.socket, read_buffer));
  TEST_ASSERT_EQUAL(LIFI_SOCKET_RECEIVING, fixture.recipient.socket.state);

  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_FALSE(LiFi_Socket_Read(&fixture.recipient.socket, read_buffer));
  TEST_ASSERT_NOT_EQUAL(LIFI_SOCKET_IDLE, fixture.recipient.socket.state);
}

void Test_LiFi_Protocol_Run(void) {
  RUN_TEST(test_transmit_payload);
  RUN_TEST(test_transmit_payload__wrong_crc);
  RUN_TEST(test_transmit_payload__socket_is_reset_after_retries_limit);
  RUN_TEST(test_transmit_payload__receiver_ignores_package_on_wrong_start_byte);
  RUN_TEST(test_socket_continue_transmission_after_confirmation);
  RUN_TEST(test_transmit_payload__confirmation_timeout);
  RUN_TEST(test_receive_payload__connection_timeout);
  RUN_TEST(test_transmit_payload__cant_transmit_to_the_busy_socket);
  RUN_TEST(test_receive_payload__cant_start_another_read_while_receiving);
}
