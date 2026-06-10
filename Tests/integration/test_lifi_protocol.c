#include "unity.h"
#include "lifi_protocol.h"
#include "fake_lifi_transport.h"

void setUp(void)
{
    Fake_LiFi_Link_Reset();
}

void tearDown(void)
{
}

void test_socket_can_send_and_receive_payload_through_loopback(void)
{
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

    TEST_ASSERT_EQUAL_UINT8('H', read_buffer[0]);
    TEST_ASSERT_EQUAL_UINT8('i', read_buffer[1]);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_socket_can_send_and_receive_payload_through_loopback);

    return UNITY_END();
}