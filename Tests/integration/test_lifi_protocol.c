#include <stdint.h>
#include <string.h>

#include "fake_lifi_transport.h"
#include "fff.h"
#include "lifi_protocol.h"
#include "unity.h"

DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(LiFi_Transmitter_TransmitBuffer, LiFi_Transmitter_t *, const uint8_t *, uint8_t);
FAKE_VOID_FUNC(LiFi_Transmitter_ToConfirmationMode, LiFi_Transmitter_t *);
FAKE_VOID_FUNC(Mock_LiFi_Socket_onErrorCallback, LiFi_Socket_Error_t, LiFi_Socket_t *);
FAKE_VOID_FUNC(Mock_LiFi_Socket_onTransmissionSuccessfulCallback, LiFi_Socket_t *);
FAKE_VOID_FUNC(Mock_LiFi_Socket_onReceiveSuccessfulCallback, LiFi_Socket_t *);

void setUp(void) {
  RESET_FAKE(LiFi_Transmitter_TransmitBuffer);
  RESET_FAKE(LiFi_Transmitter_ToConfirmationMode);
  RESET_FAKE(Mock_LiFi_Socket_onErrorCallback);
  RESET_FAKE(Mock_LiFi_Socket_onTransmissionSuccessfulCallback);
  RESET_FAKE(Mock_LiFi_Socket_onReceiveSuccessfulCallback);
  FFF_RESET_HISTORY();

  Fake_LiFi_Link_Reset();
  LiFi_Transmitter_TransmitBuffer_fake.custom_fake = Fake_LiFi_Transmitter_TransmitBuffer_Callback;
}

void tearDown(void) {
}

void test_transmit_payload(void) {
  LiFi_Transmitter_t client_transmitter = {0};
  LiFi_Receiver_t client_receiver = {0};
  LiFi_Socket_t client_socket = {0};

  LiFi_Transmitter_t server_transmitter = {0};
  LiFi_Receiver_t server_receiver = {0};
  LiFi_Socket_t server_socket = {0};

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver, NULL,
                   Mock_LiFi_Socket_onTransmissionSuccessfulCallback, NULL);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver, NULL, NULL,
                   Mock_LiFi_Socket_onReceiveSuccessfulCallback);
  Fake_LiFi_Link_Register(&client_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&server_transmitter, &client_receiver);

  uint8_t client_payload[] = {'H', 'i'};
  uint8_t read_buffer[2] = {0};

  LiFi_Socket_Read(&server_socket, read_buffer);
  LiFi_Socket_Send(&client_socket, client_payload, sizeof(client_payload));
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8('H', read_buffer[0]);
  TEST_ASSERT_EQUAL_UINT8('i', read_buffer[1]);

  Fake_LiFi_RunUntilIdle();

  // success callback had been called for client
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&client_socket,
                        Mock_LiFi_Socket_onTransmissionSuccessfulCallback_fake.arg0_val);

  Fake_LiFi_RunUntilIdle();

  // success callback had been called for server
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onReceiveSuccessfulCallback_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&server_socket, Mock_LiFi_Socket_onReceiveSuccessfulCallback_fake.arg0_val);
}

void test_transmit_payload__wrong_crc(void) {
  LiFi_Transmitter_t client_transmitter = {0};
  LiFi_Receiver_t client_receiver = {0};
  LiFi_Socket_t client_socket = {0};

  LiFi_Transmitter_t server_transmitter = {0};
  LiFi_Receiver_t server_receiver = {0};
  LiFi_Socket_t server_socket = {0};

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver, NULL, NULL, NULL);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver, NULL, NULL, NULL);
  Fake_LiFi_Link_Register(&client_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&server_transmitter, &client_receiver);

  uint8_t client_payload[] = {'H', 'i'};
  uint8_t read_buffer[2] = {0};

  LiFi_Socket_Read(&server_socket, read_buffer);
  LiFi_Socket_Send(&client_socket, client_payload, sizeof(client_payload));

  client_transmitter.tx_buffer[6] = 255;
  Fake_LiFi_RunUntilIdle();

  // server socket is prepared NAK package
  TEST_ASSERT_EQUAL_UINT8(server_transmitter.tx_buffer[2], PACKAGE_TYPE_NAK);
  TEST_ASSERT_EQUAL_UINT8(server_transmitter.tx_buffer[4], 0);

  Fake_LiFi_RunUntilIdle();

  // client socket received NAK and is going to send same payload
  TEST_ASSERT_FALSE(client_socket.is_tx_confirmation_required);
  TEST_ASSERT_EQUAL_UINT8(client_socket.rx_package[1], PACKAGE_TYPE_NAK);
  TEST_ASSERT_EQUAL_UINT8(client_socket.rx_package[3], 0);

  TEST_ASSERT_EQUAL_UINT8(server_socket.rx_package_bytes_received, 0);
  TEST_ASSERT_EQUAL_UINT8(client_socket.tx_retries_count, 1);
}

void test_transmit_payload__socket_is_reset_after_retries_limit(void) {
  LiFi_Transmitter_t client_transmitter = {0};
  LiFi_Receiver_t client_receiver = {0};
  LiFi_Socket_t client_socket = {0};

  LiFi_Transmitter_t server_transmitter = {0};
  LiFi_Receiver_t server_receiver = {0};
  LiFi_Socket_t server_socket = {0};

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver,
                   Mock_LiFi_Socket_onErrorCallback, NULL, NULL);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver, NULL, NULL, NULL);
  Fake_LiFi_Link_Register(&client_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&server_transmitter, &client_receiver);

  uint8_t client_payload[] = {'H', 'i'};
  uint8_t read_buffer[2] = {0};

  LiFi_Socket_Read(&server_socket, read_buffer);
  LiFi_Socket_Send(&client_socket, client_payload, sizeof(client_payload));

  client_transmitter.tx_buffer[6] = 255;
  client_socket.tx_retries_count = MAX_TRANSMIT_RETRIES_COUNT - 1;

  Fake_LiFi_RunUntilIdle(); // 1st iteration is sending from client to server
  Fake_LiFi_RunUntilIdle(); // 2nd iteration is sending confirmation from server to client

  // client socket received NAK and socket is reset
  TEST_ASSERT_FALSE(client_socket.is_tx_confirmation_required);
  TEST_ASSERT_EQUAL_UINT8(client_socket.rx_package[1], PACKAGE_TYPE_NAK);
  TEST_ASSERT_EQUAL_UINT8(client_socket.rx_package[3], 0);
  TEST_ASSERT_EQUAL_UINT8(server_socket.rx_package_bytes_received, 0);
  TEST_ASSERT_EQUAL_UINT8(client_socket.tx_retries_count, 0);
  TEST_ASSERT_FALSE(client_socket.is_busy);
  TEST_ASSERT_EQUAL_UINT(1, Mock_LiFi_Socket_onErrorCallback_fake.call_count);
  TEST_ASSERT_EQUAL(LIFI_SOCKET_CONNECTION_ERROR, Mock_LiFi_Socket_onErrorCallback_fake.arg0_val);
  TEST_ASSERT_EQUAL_PTR(&client_socket, Mock_LiFi_Socket_onErrorCallback_fake.arg1_val);
}

void test_transmit_payload__receiver_ignores_package_on_wrong_start_byte(void) {
  LiFi_Transmitter_t client_transmitter = {0};
  LiFi_Receiver_t client_receiver = {0};
  LiFi_Socket_t client_socket = {0};

  LiFi_Transmitter_t server_transmitter = {0};
  LiFi_Receiver_t server_receiver = {0};
  LiFi_Socket_t server_socket = {0};

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver, NULL, NULL, NULL);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver, NULL, NULL, NULL);
  Fake_LiFi_Link_Register(&client_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&server_transmitter, &client_receiver);

  uint8_t client_payload[] = {'H', 'i'};
  uint8_t read_buffer[2] = {0};

  LiFi_Socket_Read(&server_socket, read_buffer);
  LiFi_Socket_Send(&client_socket, client_payload, sizeof(client_payload));

  client_transmitter.tx_buffer[1] = 0;
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8(0, read_buffer[0]);
  TEST_ASSERT_EQUAL_UINT8(0, read_buffer[1]);
}

void test_socket_continue_transmission_after_confirmation(void) {
  LiFi_Transmitter_t client_transmitter = {0};
  LiFi_Receiver_t client_receiver = {0};
  LiFi_Socket_t client_socket = {0};

  LiFi_Transmitter_t server_transmitter = {0};
  LiFi_Receiver_t server_receiver = {0};
  LiFi_Socket_t server_socket = {0};

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver, NULL, NULL, NULL);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver, NULL, NULL, NULL);
  Fake_LiFi_Link_Register(&client_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&server_transmitter, &client_receiver);

  uint8_t client_payload[] = {
      'L', 'o', 'r', 'e', 'm', ' ', 'i', 'p', 's', 'u', 'm', ' ', 'd', 'o', 'l', 'o', 'r', ' ', 's',
      'i', 't', ' ', 'a', 'm', 'e', 't', ',', ' ', 'c', 'o', 'n', 's', 'e', 'c', 't', 'e', 't', 'u',
      'r', ' ', 'a', 'd', 'i', 'p', 'i', 's', 'c', 'i', 'n', 'g', ' ', 'e', 'l', 'i', 't', ',', ' ',
      's', 'e', 'd', ' ', 'd', 'o', ' ', 'e', 'i', 'u', 's', 'm', 'o', 'd', ' ', 't', 'e', 'm', 'p',
      'o', 'r', ' ', 'i', 'n', 'c', 'i', 'd', 'i', 'd', 'u', 'n', 't', ' ', 'u', 't'};
  uint8_t server_buffer[sizeof(client_payload)] = {0};

  LiFi_Socket_Read(&server_socket, server_buffer);
  LiFi_Socket_Send(&client_socket, client_payload, sizeof(client_payload));
  TEST_ASSERT_EQUAL_UINT8(client_socket.tx_package_id, 1);

  Fake_LiFi_RunUntilIdle();

  // client socket sent payload and switched to the confirmation mode
  TEST_ASSERT_TRUE(client_socket.is_tx_confirmation_required);
  TEST_ASSERT_EQUAL_UINT(1, LiFi_Transmitter_ToConfirmationMode_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&client_transmitter, LiFi_Transmitter_ToConfirmationMode_fake.arg0_val);

  // server socket is prepared ACK package
  TEST_ASSERT_EQUAL_UINT8(server_transmitter.tx_buffer[TX_PACKAGE_PACKAGE_TYPE_INDEX],
                          PACKAGE_TYPE_ACK);
  TEST_ASSERT_EQUAL_UINT8(server_transmitter.tx_buffer[TX_PACKAGE_LENGTH_INDEX], 0);
  TEST_ASSERT_EQUAL_UINT8(server_socket.rx_package_id, 1);
  TEST_ASSERT_EQUAL_UINT8(server_socket.tx_package_id, 1);

  Fake_LiFi_RunUntilIdle();

  // client socket received ACK and prepared next package to send
  TEST_ASSERT_FALSE(client_socket.is_tx_confirmation_required);
  TEST_ASSERT_EQUAL_UINT8(client_socket.rx_package[RX_PACKAGE_PACKAGE_TYPE_INDEX],
                          PACKAGE_TYPE_ACK);
  TEST_ASSERT_EQUAL_UINT8(client_socket.rx_package[RX_PACKAGE_LENGTH_INDEX], 0);

  TEST_ASSERT_EQUAL_MEMORY(&client_transmitter.tx_buffer[TX_PACKAGE_HEADER_BYTES],
                           &client_payload[client_socket.tx_bytes_processed],
                           LIFI_TX_BUFFER_SIZE - TX_PACKAGE_HEADER_BYTES - 1);

  TEST_ASSERT_EQUAL_UINT8(client_socket.tx_package_id, 2);
}

void test_transmit_payload__confirmation_timeout(void) {
  LiFi_Transmitter_t client_transmitter = {0};
  LiFi_Receiver_t client_receiver = {0};
  LiFi_Socket_t client_socket = {0};

  LiFi_Transmitter_t server_transmitter = {0};
  LiFi_Receiver_t server_receiver = {0};
  LiFi_Socket_t server_socket = {0};

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver,
                   Mock_LiFi_Socket_onErrorCallback, NULL, NULL);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver, NULL, NULL, NULL);
  Fake_LiFi_Link_Register(&client_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&server_transmitter, &client_receiver);

  uint8_t client_payload[] = {'H', 'i'};
  uint8_t read_buffer[2] = {0};

  LiFi_Socket_Read(&server_socket, read_buffer);
  LiFi_Socket_Send(&client_socket, client_payload, sizeof(client_payload));
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8('H', read_buffer[0]);
  TEST_ASSERT_EQUAL_UINT8('i', read_buffer[1]);

  TEST_ASSERT_TRUE(client_socket.is_tx_confirmation_required);

  // emulate timeout
  LiFi_Transmitter_TimerCallback(&client_transmitter);

  // socket is reset and ready for process new
  TEST_ASSERT_FALSE(client_socket.is_tx_confirmation_required);
  TEST_ASSERT_NULL(client_socket.tx_buffer);
  TEST_ASSERT_EQUAL_UINT8(client_socket.tx_buffer_length, 0);
  TEST_ASSERT_EQUAL_UINT8(client_socket.tx_bytes_processed, 0);
  TEST_ASSERT_FALSE(client_socket.is_busy);

  TEST_ASSERT_EQUAL_PTR(&client_socket, Mock_LiFi_Socket_onErrorCallback_fake.arg1_val);
}

void test_receive_payload__connection_timeout(void) {
  LiFi_Transmitter_t client_transmitter = {0};
  LiFi_Receiver_t client_receiver = {0};
  LiFi_Socket_t client_socket = {0};

  LiFi_Transmitter_t server_transmitter = {0};
  LiFi_Receiver_t server_receiver = {0};
  LiFi_Socket_t server_socket = {0};

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver,
                   Mock_LiFi_Socket_onErrorCallback, NULL, NULL);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver, NULL, NULL, NULL);
  Fake_LiFi_Link_Register(&client_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&server_transmitter, &client_receiver);

  uint8_t client_payload[] = {'H', 'i'};
  uint8_t read_buffer[2] = {0};

  LiFi_Socket_Read(&server_socket, read_buffer);
  LiFi_Socket_Send(&client_socket, client_payload, sizeof(client_payload));
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8('H', read_buffer[0]);
  TEST_ASSERT_EQUAL_UINT8('i', read_buffer[1]);

  // emulate timeout
  LiFi_Receiver_TimerCallback(&server_receiver);

  // socket is reset and ready for process new
  TEST_ASSERT_NULL(server_socket.rx_buffer);
  TEST_ASSERT_EQUAL_UINT8(server_socket.rx_package_bytes_received, 0);
  TEST_ASSERT_FALSE(server_socket.is_busy);
}

void test_transmit_payload__cant_transmit_to_the_busy_socket(void) {
  LiFi_Transmitter_t client_transmitter = {0};
  LiFi_Receiver_t client_receiver = {0};
  LiFi_Socket_t client_socket = {0};

  LiFi_Transmitter_t server_transmitter = {0};
  LiFi_Receiver_t server_receiver = {0};
  LiFi_Socket_t server_socket = {0};

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver,
                   Mock_LiFi_Socket_onErrorCallback, NULL, NULL);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver, NULL, NULL, NULL);
  Fake_LiFi_Link_Register(&client_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&server_transmitter, &client_receiver);

  uint8_t client_payload[] = {'H', 'i'};
  uint8_t another_client_payload[] = {'W', 'a', 't', '?'};
  uint8_t read_buffer[2] = {0};

  LiFi_Socket_Read(&server_socket, read_buffer);
  LiFi_Socket_Send(&client_socket, client_payload, sizeof(client_payload));

  TEST_ASSERT_FALSE(
      LiFi_Socket_Send(&client_socket, another_client_payload, sizeof(another_client_payload)));
}

void test_receive_payload__not_busy_on_read(void) {
  LiFi_Transmitter_t client_transmitter = {0};
  LiFi_Receiver_t client_receiver = {0};
  LiFi_Socket_t client_socket = {0};

  LiFi_Transmitter_t server_transmitter = {0};
  LiFi_Receiver_t server_receiver = {0};
  LiFi_Socket_t server_socket = {0};

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver,
                   Mock_LiFi_Socket_onErrorCallback, NULL, NULL);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver, NULL, NULL, NULL);
  Fake_LiFi_Link_Register(&client_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&server_transmitter, &client_receiver);

  uint8_t client_payload[] = {'H', 'i'};
  uint8_t read_buffer[2] = {0};

  TEST_ASSERT_TRUE(LiFi_Socket_Read(&server_socket, read_buffer));
  TEST_ASSERT_TRUE(LiFi_Socket_Read(&server_socket, read_buffer));
  TEST_ASSERT_FALSE(server_socket.is_busy);

  LiFi_Socket_Send(&client_socket, client_payload, sizeof(client_payload));
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_FALSE(LiFi_Socket_Read(&server_socket, read_buffer));
  TEST_ASSERT_TRUE(server_socket.is_busy);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_transmit_payload);
  RUN_TEST(test_transmit_payload__wrong_crc);
  RUN_TEST(test_transmit_payload__socket_is_reset_after_retries_limit);
  RUN_TEST(test_transmit_payload__receiver_ignores_package_on_wrong_start_byte);
  RUN_TEST(test_socket_continue_transmission_after_confirmation);
  RUN_TEST(test_transmit_payload__confirmation_timeout);
  RUN_TEST(test_receive_payload__connection_timeout);
  RUN_TEST(test_transmit_payload__cant_transmit_to_the_busy_socket);
  RUN_TEST(test_receive_payload__not_busy_on_read);

  return UNITY_END();
}
