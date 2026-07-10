#include <stdint.h>

#include "fake_lifi_callbacks.h"
#include "fake_lifi_transport.h"
#include "lifi_protocol.h"
#include "lifi_socket_fixture.h"
#include "lifi_transmitter.h"
#include "test_suites.h"
#include "unity.h"

static uint8_t test_calculate_crc(const uint8_t *buffer, uint8_t length) {
  uint8_t crc = 0;

  while (length--) {
    crc ^= *buffer++;
    for (uint8_t i = 0; i < 8; ++i) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
  }

  return crc;
}

static void replace_current_tx_package_type(LiFi_Transmitter_t *transmitter,
                                            PackageType_t package_type) {
  uint8_t payload_length = transmitter->tx_buffer[TX_PACKAGE_LENGTH_INDEX];
  uint8_t crc_index = TX_PACKAGE_HEADER_BYTES + payload_length;

  transmitter->tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX] = (uint8_t)package_type;
  transmitter->tx_buffer[crc_index] =
      test_calculate_crc(transmitter->tx_buffer + TX_PACKAGE_PACKAGE_TYPE_INDEX,
                         payload_length + TX_PACKAGE_HEADER_BYTES - 2);
}

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
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&fixture.sender.socket,
                        Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.arg0_val);

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
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, fixture.recipient.socket.rx_buffer, sizeof(payload));
  TEST_ASSERT_EQUAL_UINT8(sizeof(payload), fixture.recipient.socket.rx_bytes_received);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.recipient.socket.state);
}

void test_transmit_payload__ack_busy_pauses_sender(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_CONFIRMATION, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT8(
      PACKAGE_TYPE_ACK_READY,
      fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX]);

  // Simulate receiver-side backpressure: the payload was accepted, but the socket paused RX before
  // the pending ACK is delivered. The peer sees this as ACK_BUSY and must stop sending.
  fixture.recipient.socket.state = LIFI_SOCKET_RX_PAUSED;
  replace_current_tx_package_type(fixture.recipient.socket.transmitter, PACKAGE_TYPE_ACK_BUSY);
  uint8_t transmit_call_count_before_ack_busy = LiFi_Transmitter_TransmitBuffer_fake.call_count;

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_RX_PAUSED, fixture.recipient.socket.state);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT8(transmit_call_count_before_ack_busy,
                          LiFi_Transmitter_TransmitBuffer_fake.call_count);
  TEST_ASSERT_EQUAL_UINT8(sizeof(payload), fixture.sender.socket.tx_bytes_processed);
  TEST_ASSERT_FALSE(fixture.sender.socket.transmitter->is_busy);
  TEST_ASSERT_EQUAL_UINT(0, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
}

void test_transmit_payload__waiting_sender_sends_status_on_timeout__continue_waiting(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));

  Fake_LiFi_RunUntilIdle();

  // Simulate receiver-side backpressure: the payload was accepted, but the socket paused RX before
  // the pending ACK is delivered. The peer sees this as ACK_BUSY and must poll with STATUS later.
  fixture.recipient.socket.state = LIFI_SOCKET_RX_PAUSED;
  replace_current_tx_package_type(fixture.recipient.socket.transmitter, PACKAGE_TYPE_ACK_BUSY);
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_RX_PAUSED, fixture.recipient.socket.state);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);

  LiFi_Transmitter_TimerCallback(fixture.sender.socket.transmitter);

  TEST_ASSERT_EQUAL(LIFI_SOCKET_SENDING_CONTROL, fixture.sender.socket.state);
  TEST_ASSERT_TRUE(fixture.sender.socket.transmitter->is_busy);
  TEST_ASSERT_EQUAL_UINT8(
      PACKAGE_TYPE_STATUS,
      fixture.sender.socket.transmitter->tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(fixture.sender.socket.tx_package_id,
                          fixture.sender.socket.transmitter->tx_buffer[TX_PACKAGE_ID_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.transmitter->tx_buffer[TX_PACKAGE_LENGTH_INDEX]);

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_SENDING_CONTROL_BUSY, fixture.recipient.socket.state);
  TEST_ASSERT_EQUAL_UINT8(
      PACKAGE_TYPE_ACK_BUSY,
      fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(fixture.recipient.socket.rx_package_id,
                          fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_ID_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0,
                          fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_LENGTH_INDEX]);

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8(fixture.sender.socket.tx_package_id,
                          fixture.sender.socket.rx_package[RX_PACKAGE_ID_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.rx_package[RX_PACKAGE_LENGTH_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_ACK_BUSY,
                          fixture.sender.socket.rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);
}

void test_transmit_payload__waiting_sender_sends_status_on_timeout__resume_transmission(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));

  Fake_LiFi_RunUntilIdle();

  // Simulate receiver-side backpressure: the payload was accepted, but the socket paused RX before
  // the pending ACK is delivered. The peer sees this as ACK_BUSY and must poll with STATUS later.
  fixture.recipient.socket.state = LIFI_SOCKET_RX_PAUSED;
  replace_current_tx_package_type(fixture.recipient.socket.transmitter, PACKAGE_TYPE_ACK_BUSY);
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_RX_PAUSED, fixture.recipient.socket.state);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);

  LiFi_Transmitter_TimerCallback(fixture.sender.socket.transmitter);

  // Simulate receiver is ready to receive next packages
  fixture.recipient.socket.state = LIFI_SOCKET_RECEIVING;

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_SENDING_CONTROL, fixture.recipient.socket.state);
  TEST_ASSERT_EQUAL_UINT8(
      PACKAGE_TYPE_READY,
      fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(fixture.recipient.socket.rx_package_id,
                          fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_ID_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0,
                          fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_LENGTH_INDEX]);

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8(LIFI_SOCKET_RECEIVING, fixture.recipient.socket.state);

  TEST_ASSERT_EQUAL_UINT8(fixture.sender.socket.tx_package_id,
                          fixture.sender.socket.rx_package[RX_PACKAGE_ID_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.rx_package[RX_PACKAGE_LENGTH_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_READY,
                          fixture.sender.socket.rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(LIFI_SOCKET_TRANSMITTING, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT8(
      PACKAGE_TYPE_EOT,
      fixture.sender.socket.transmitter->tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX]);

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_EOT,
                          fixture.recipient.socket.rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&fixture.sender.socket,
                        Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.arg0_val);
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

void test_transmit_payload__sender_sends_package_receiver_busy(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));

  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  // Simulate receiver got paused unexpectedly
  fixture.recipient.socket.state = LIFI_SOCKET_RX_PAUSED;
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_SENDING_CONTROL_BUSY, fixture.recipient.socket.state);
  TEST_ASSERT_EQUAL_UINT8(
      PACKAGE_TYPE_BUSY,
      fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(fixture.recipient.socket.rx_package_id,
                          fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_ID_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0,
                          fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_LENGTH_INDEX]);

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8(LIFI_SOCKET_RX_PAUSED, fixture.recipient.socket.state);

  // sender got BUSY package type and waiting until ready
  TEST_ASSERT_EQUAL_UINT8(fixture.sender.socket.tx_package_id,
                          fixture.sender.socket.rx_package[RX_PACKAGE_ID_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.rx_package[RX_PACKAGE_LENGTH_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_BUSY,
                          fixture.sender.socket.rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT8(
      PACKAGE_TYPE_EOT,
      fixture.sender.socket.transmitter->tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX]);
}

void test_transmit_payload__eot_not_acked(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, fixture.recipient.socket.rx_buffer, sizeof(payload));

  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  // Original EOT has been processed: receiver already fired its success callback and is holding
  // the ACK_READY, about to transmit it.
  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_CONFIRMATION, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_EOT,
                          fixture.sender.socket.tx_package[TX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_ACK_EOD, fixture.recipient.socket.state);
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onReceiveSuccessfulCallback_fake.call_count);

  replace_current_tx_package_type(fixture.recipient.socket.transmitter, PACKAGE_TYPE_NAK);

  Fake_LiFi_RunUntilIdle();

  // Since EOD had been NACKed, callback had not been executed, EOD retry happened
  TEST_ASSERT_EQUAL_UINT(0, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_UINT8(1, fixture.sender.socket.tx_retries_count);

  // Retry resent an EOT (not a bogus empty PAYLOAD), and by the time it lands the receiver has
  // already completed and moved on to IDLE.
  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_EOT,
                          fixture.sender.socket.tx_package[TX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.recipient.socket.state);

  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&fixture.sender.socket,
                        Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.arg0_val);

  // The retried EOT was recognized as a duplicate of an already-completed transfer: receiver
  // re-acked it without reprocessing/re-firing its own success callback.
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onReceiveSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.recipient.socket.state);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_retries_count);
}

void test_transmit_payload__eot_confirmation_has_wrong_crc(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, fixture.recipient.socket.rx_buffer, sizeof(payload));

  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  // Corrupt the CRC byte in EOD confirmation.
  fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_HEADER_BYTES] ^= 0xFF;

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT(0, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_UINT8(1, fixture.sender.socket.tx_retries_count);

  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&fixture.sender.socket,
                        Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.arg0_val);
}

void test_transmit_payload__eot_not_confirmed_resend_on_timeout(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, fixture.recipient.socket.rx_buffer, sizeof(payload));

  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  LiFi_Transmitter_TimerCallback(&fixture.sender.transmitter);

  TEST_ASSERT_EQUAL_UINT(0, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_UINT8(1, fixture.sender.socket.tx_retries_count);

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&fixture.sender.socket,
                        Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.arg0_val);
}

void test_transmit_payload__eot_confirmation_busy_pauses_sender(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_TRANSMITTING, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT8(
      PACKAGE_TYPE_EOT,
      fixture.sender.socket.transmitter->tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX]);

  // Simulate receiver-side backpressure right as the EOT arrives: it gets paused before it can
  // process/ack it, and replies BUSY instead of ACK_READY.
  fixture.recipient.socket.state = LIFI_SOCKET_RX_PAUSED;
  fixture.sender.socket.tx_retries_count = 2; // will get reset on receiving BUSY

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_SENDING_CONTROL_BUSY, fixture.recipient.socket.state);
  TEST_ASSERT_EQUAL_UINT8(
      PACKAGE_TYPE_BUSY,
      fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX]);

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8(fixture.sender.socket.tx_package_id,
                          fixture.sender.socket.rx_package[RX_PACKAGE_ID_INDEX]);
  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_BUSY,
                          fixture.sender.socket.rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_retries_count);
  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_EOT,
                          fixture.sender.socket.tx_package[TX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT(0, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
}

void test_transmit_payload__eot_confirmation_ack_busy_completes_transfer(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_CONFIRMATION, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_EOT,
                          fixture.sender.socket.tx_package[TX_PACKAGE_PACKAGE_TYPE_INDEX]);

  // Simulate the receiver pausing right as it was about to ack the EOT: the pending ACK_READY
  // gets downgraded to ACK_BUSY before it's actually sent. Unlike plain BUSY, ACK_BUSY still
  // confirms the EOT was received, so the transfer is already complete - there's nothing left to
  // resume once the receiver unpauses.
  fixture.recipient.socket.state = LIFI_SOCKET_RX_PAUSED;
  replace_current_tx_package_type(fixture.recipient.socket.transmitter, PACKAGE_TYPE_ACK_BUSY);

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.sender.socket.state);
  TEST_ASSERT_NULL(fixture.sender.socket.tx_buffer);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_buffer_length);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_retries_count);
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&fixture.sender.socket,
                        Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.arg0_val);
}

void test_transmit_payload__eot_confirmation_retries_exhausted(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_CONFIRMATION, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT8(PACKAGE_TYPE_EOT,
                          fixture.sender.socket.tx_package[TX_PACKAGE_PACKAGE_TYPE_INDEX]);
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onReceiveSuccessfulCallback_fake.call_count);

  // Corrupt the EOT's confirmation and make this the retry that exhausts the limit.
  replace_current_tx_package_type(fixture.recipient.socket.transmitter, PACKAGE_TYPE_NAK);
  fixture.sender.socket.tx_retries_count = MAX_TRANSMIT_RETRIES_COUNT - 1;

  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_retries_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.sender.socket.state);
  TEST_ASSERT_NULL(fixture.sender.socket.tx_buffer);
  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_buffer_length);
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onErrorCallback_fake.call_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_CONNECTION_ERROR, Mock_LiFi_Socket_onErrorCallback_fake.arg0_val);
  TEST_ASSERT_EQUAL_PTR(&fixture.sender.socket, Mock_LiFi_Socket_onErrorCallback_fake.arg1_val);
  TEST_ASSERT_EQUAL_UINT(0, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);

  // Receiver, meanwhile, already completed successfully before the sender gave up - the two
  // sides disagree once retries are exhausted on a link that never lets the final ACK through.
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onReceiveSuccessfulCallback_fake.call_count);
}

void test_transmit_payload__idle_receiver_drops_eot_retry_with_mismatched_id(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();
  Fake_LiFi_RunUntilIdle();

  // Original EOT already completed the receive side; receiver is holding its pending ACK_READY.
  TEST_ASSERT_EQUAL(LIFI_SOCKET_ACK_EOD, fixture.recipient.socket.state);

  replace_current_tx_package_type(fixture.recipient.socket.transmitter, PACKAGE_TYPE_NAK);
  Fake_LiFi_RunUntilIdle();

  // Receiver moved to IDLE once its (corrupted-to-NAK) ack finished transmitting; sender queued a
  // retry EOT carrying the same id it has used for this transfer all along.
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.recipient.socket.state);

  // Simulate the receiver no longer recognizing this id as its last completed transfer (e.g. a
  // stale/foreign retry unrelated to what it just finished).
  fixture.recipient.socket.rx_package_id = 99;

  Fake_LiFi_RunUntilIdle();

  // Mismatched id: receiver silently drops it - no ack, no re-fired callback.
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.recipient.socket.state);
  TEST_ASSERT_FALSE(fixture.recipient.socket.transmitter->is_busy);
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onReceiveSuccessfulCallback_fake.call_count);

  // Sender is left waiting for a confirmation that will never come.
  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_CONFIRMATION, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT(0, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
}

void test_transmit_payload__waiting_ready_timeout_retries_exhausted(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();

  fixture.recipient.socket.state = LIFI_SOCKET_RX_PAUSED;
  replace_current_tx_package_type(fixture.recipient.socket.transmitter, PACKAGE_TYPE_ACK_BUSY);
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);

  // Receiver goes fully silent from here - every future STATUS poll times out with no reply.
  fixture.sender.socket.tx_retries_count = MAX_TRANSMIT_RETRIES_COUNT - 1;
  LiFi_Transmitter_TimerCallback(fixture.sender.socket.transmitter);

  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_retries_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.sender.socket.state);
  TEST_ASSERT_NULL(fixture.sender.socket.tx_buffer);
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onErrorCallback_fake.call_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_CONNECTION_ERROR, Mock_LiFi_Socket_onErrorCallback_fake.arg0_val);
  TEST_ASSERT_EQUAL_PTR(&fixture.sender.socket, Mock_LiFi_Socket_onErrorCallback_fake.arg1_val);
}

void test_transmit_payload__waiting_ready_nak_retries_exhausted(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();

  fixture.recipient.socket.state = LIFI_SOCKET_RX_PAUSED;
  replace_current_tx_package_type(fixture.recipient.socket.transmitter, PACKAGE_TYPE_ACK_BUSY);
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);

  LiFi_Transmitter_TimerCallback(fixture.sender.socket.transmitter); // sends a STATUS poll

  // Simulate a desynced id from here on: every STATUS poll gets rejected with NAK instead of
  // ACK_BUSY/READY - an active rejection, not proof it's fine to keep waiting.
  fixture.recipient.socket.rx_package_id = 99;
  fixture.sender.socket.tx_retries_count = MAX_TRANSMIT_RETRIES_COUNT - 1;

  Fake_LiFi_RunUntilIdle(); // STATUS delivered to recipient -> NAK queued
  Fake_LiFi_RunUntilIdle(); // NAK delivered to sender -> retry budget exhausted

  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_retries_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onErrorCallback_fake.call_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_CONNECTION_ERROR, Mock_LiFi_Socket_onErrorCallback_fake.arg0_val);
}

void test_transmit_payload__waiting_ready_garbled_response_retries_exhausted(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();

  fixture.recipient.socket.state = LIFI_SOCKET_RX_PAUSED;
  replace_current_tx_package_type(fixture.recipient.socket.transmitter, PACKAGE_TYPE_ACK_BUSY);
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);

  LiFi_Transmitter_TimerCallback(fixture.sender.socket.transmitter); // sends a STATUS poll
  Fake_LiFi_RunUntilIdle(); // STATUS delivered, recipient (still RX_PAUSED) replies ACK_BUSY

  // Corrupt this ACK_BUSY reply's CRC before it lands - garbled bytes are not a confirmed
  // ACK_BUSY, so they must not be treated as proof it's fine to keep waiting either.
  fixture.recipient.socket.transmitter->tx_buffer[TX_PACKAGE_HEADER_BYTES] ^= 0xFF;
  fixture.sender.socket.tx_retries_count = MAX_TRANSMIT_RETRIES_COUNT - 1;

  Fake_LiFi_RunUntilIdle(); // garbled ACK_BUSY delivered to sender -> retry budget exhausted

  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_retries_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_IDLE, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onErrorCallback_fake.call_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_CONNECTION_ERROR, Mock_LiFi_Socket_onErrorCallback_fake.arg0_val);
}

void test_transmit_payload__waiting_ready_ack_busy_resets_retries(void) {
  LiFi_Socket_Pair_Fixture_t fixture;
  LiFi_Socket_Pair_Fixture_Init(&fixture);
  uint8_t payload[] = {'H', 'i'};
  uint8_t read_buffer[sizeof(payload)] = {0};

  LiFi_Socket_Read(&fixture.recipient.socket, read_buffer);
  LiFi_Socket_Send(&fixture.sender.socket, payload, sizeof(payload));
  Fake_LiFi_RunUntilIdle();

  fixture.recipient.socket.state = LIFI_SOCKET_RX_PAUSED;
  replace_current_tx_package_type(fixture.recipient.socket.transmitter, PACKAGE_TYPE_ACK_BUSY);
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);

  LiFi_Transmitter_TimerCallback(fixture.sender.socket.transmitter); // sends a STATUS poll

  // Right at the edge of the retry budget - a genuine ACK_BUSY must still save it, no matter how
  // long the receiver has legitimately been busy.
  fixture.sender.socket.tx_retries_count = MAX_TRANSMIT_RETRIES_COUNT - 1;

  Fake_LiFi_RunUntilIdle(); // STATUS delivered, recipient (still RX_PAUSED) replies ACK_BUSY
  Fake_LiFi_RunUntilIdle(); // ACK_BUSY delivered to sender

  TEST_ASSERT_EQUAL_UINT8(0, fixture.sender.socket.tx_retries_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_WAITING_READY, fixture.sender.socket.state);
  TEST_ASSERT_EQUAL_UINT(0, Mock_LiFi_Socket_onErrorCallback_fake.call_count);
}

void Test_LiFi_Protocol_Run(void) {
  RUN_TEST(test_transmit_payload);
  RUN_TEST(test_transmit_payload__wrong_crc);
  RUN_TEST(test_transmit_payload__socket_is_reset_after_retries_limit);
  RUN_TEST(test_transmit_payload__receiver_ignores_package_on_wrong_start_byte);
  RUN_TEST(test_socket_continue_transmission_after_confirmation);
  RUN_TEST(test_transmit_payload__ack_busy_pauses_sender);
  RUN_TEST(test_transmit_payload__waiting_sender_sends_status_on_timeout__continue_waiting);
  RUN_TEST(test_transmit_payload__waiting_sender_sends_status_on_timeout__resume_transmission);
  RUN_TEST(test_transmit_payload__confirmation_timeout);
  RUN_TEST(test_receive_payload__connection_timeout);
  RUN_TEST(test_transmit_payload__cant_transmit_to_the_busy_socket);
  RUN_TEST(test_receive_payload__cant_start_another_read_while_receiving);
  RUN_TEST(test_transmit_payload__sender_sends_package_receiver_busy);
  RUN_TEST(test_transmit_payload__eot_not_acked);
  RUN_TEST(test_transmit_payload__eot_confirmation_has_wrong_crc);
  RUN_TEST(test_transmit_payload__eot_not_confirmed_resend_on_timeout);
  RUN_TEST(test_transmit_payload__eot_confirmation_busy_pauses_sender);
  RUN_TEST(test_transmit_payload__eot_confirmation_ack_busy_completes_transfer);
  RUN_TEST(test_transmit_payload__eot_confirmation_retries_exhausted);
  RUN_TEST(test_transmit_payload__idle_receiver_drops_eot_retry_with_mismatched_id);
  RUN_TEST(test_transmit_payload__waiting_ready_timeout_retries_exhausted);
  RUN_TEST(test_transmit_payload__waiting_ready_nak_retries_exhausted);
  RUN_TEST(test_transmit_payload__waiting_ready_garbled_response_retries_exhausted);
  RUN_TEST(test_transmit_payload__waiting_ready_ack_busy_resets_retries);
}
