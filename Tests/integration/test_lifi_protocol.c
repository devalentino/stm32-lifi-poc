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
    LiFi_Transmitter_t transmitter = {0};
    LiFi_Receiver_t receiver = {0};
    LiFi_Socket_t socket = {0};

    uint8_t tx[] = {'H', 'i'};
    uint8_t rx[2] = {0};

    LiFi_Socket_Init(&socket, &transmitter, &receiver);
    Fake_LiFi_Link_Register(&transmitter, &receiver);

    LiFi_Socket_Read(&socket, rx);
    LiFi_Socket_Send(&socket, tx, sizeof(tx));

    TEST_ASSERT_EQUAL_UINT8('H', rx[0]);
    TEST_ASSERT_EQUAL_UINT8('i', rx[1]);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_socket_can_send_and_receive_payload_through_loopback);

    return UNITY_END();
}