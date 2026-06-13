#include <stdint.h>

#include "fake_lifi_transport.h"
#include "fff.h"
#include "lifi_protocol.h"
#include "unity.h"

DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(LiFi_Transmitter_TransmitBuffer, LiFi_Transmitter_t *, const uint8_t *, uint8_t);
FAKE_VOID_FUNC(LiFi_Transmitter_ToConfirmationMode, LiFi_Transmitter_t *);

void setUp(void) {
  RESET_FAKE(LiFi_Transmitter_TransmitBuffer);
  RESET_FAKE(LiFi_Transmitter_ToConfirmationMode);
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

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&client_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&server_transmitter, &client_receiver);

  uint8_t client_payload[] = {'H', 'i'};
  uint8_t read_buffer[2] = {0};

  LiFi_Socket_Read(&server_socket, read_buffer);
  LiFi_Socket_Send(&client_socket, client_payload, sizeof(client_payload));
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8('H', read_buffer[0]);
  TEST_ASSERT_EQUAL_UINT8('i', read_buffer[1]);
}

void test_transmit_payload__wrong_crc(void) {
  LiFi_Transmitter_t client_transmitter = {0};
  LiFi_Receiver_t client_receiver = {0};
  LiFi_Socket_t client_socket = {0};

  LiFi_Transmitter_t server_transmitter = {0};
  LiFi_Receiver_t server_receiver = {0};
  LiFi_Socket_t server_socket = {0};

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&client_transmitter, &server_receiver);
  Fake_LiFi_Link_Register(&server_transmitter, &client_receiver);

  uint8_t client_payload[] = {'H', 'i'};
  uint8_t read_buffer[2] = {0};

  LiFi_Socket_Read(&server_socket, read_buffer);
  LiFi_Socket_Send(&client_socket, client_payload, sizeof(client_payload));

  client_transmitter.tx_buffer[6] = 255;
  Fake_LiFi_RunUntilIdle();

  TEST_ASSERT_EQUAL_UINT8(0, read_buffer[0]);
  TEST_ASSERT_EQUAL_UINT8(0, read_buffer[1]);

  // server socket is prepared NAK payload, 1 symbol
  TEST_ASSERT_EQUAL_UINT8(server_transmitter.tx_buffer[3], 1);
  TEST_ASSERT_EQUAL_UINT8(server_transmitter.tx_buffer[4], NAK);

  Fake_LiFi_RunUntilIdle();

  // client socket received NAK and is going to send same payload
  TEST_ASSERT_FALSE(client_socket.is_tx_confirmation_required);
  TEST_ASSERT_EQUAL_UINT8(client_socket.rx_package[2], 1);
  TEST_ASSERT_EQUAL_UINT8(client_socket.rx_package[3], NAK);
  TEST_ASSERT_EQUAL_MEMORY(&client_transmitter.tx_buffer[4], &client_payload, 2);
  TEST_ASSERT_EQUAL_UINT8(server_socket.rx_package_bytes_received, 0);
}

void test_transmit_payload__receiver_ignores_package_on_wrong_start_byte(void) {
  LiFi_Transmitter_t client_transmitter = {0};
  LiFi_Receiver_t client_receiver = {0};
  LiFi_Socket_t client_socket = {0};

  LiFi_Transmitter_t server_transmitter = {0};
  LiFi_Receiver_t server_receiver = {0};
  LiFi_Socket_t server_socket = {0};

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver);
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

  LiFi_Socket_Init(&client_socket, &client_transmitter, &client_receiver);
  LiFi_Socket_Init(&server_socket, &server_transmitter, &server_receiver);
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

  Fake_LiFi_RunUntilIdle();

  // client socket sent payload and switched to the confirmation mode
  TEST_ASSERT_TRUE(client_socket.is_tx_confirmation_required);
  TEST_ASSERT_EQUAL_UINT(1, LiFi_Transmitter_ToConfirmationMode_fake.call_count);
  TEST_ASSERT_EQUAL_PTR(&client_transmitter, LiFi_Transmitter_ToConfirmationMode_fake.arg0_val);

  // server socket is prepared ACK payload, 1 symbol
  TEST_ASSERT_EQUAL_UINT8(server_transmitter.tx_buffer[3], 1);
  TEST_ASSERT_EQUAL_UINT8(server_transmitter.tx_buffer[4], ACK);

  Fake_LiFi_RunUntilIdle();

  // client socket received ACK and prepared next package to send
  TEST_ASSERT_FALSE(client_socket.is_tx_confirmation_required);
  TEST_ASSERT_EQUAL_UINT8(client_socket.rx_package[2], 1);
  TEST_ASSERT_EQUAL_UINT8(client_socket.rx_package[3], ACK);
  TEST_ASSERT_EQUAL_MEMORY(&client_transmitter.tx_buffer[4], &client_payload[35], 35);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_transmit_payload);
  RUN_TEST(test_transmit_payload__wrong_crc);
  RUN_TEST(test_transmit_payload__receiver_ignores_package_on_wrong_start_byte);
  RUN_TEST(test_socket_continue_transmission_after_confirmation);

  return UNITY_END();
}
