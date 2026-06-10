#include "unity.h"
#include <stdint.h>
#include "fake_lifi_transport.h"

#define FAKE_LIFI_MAX_LINKS 5
static Fake_LiFi_Link_t registered_links[FAKE_LIFI_MAX_LINKS];

void Fake_LiFi_Link_Reset(void)
{
    for (uint8_t i = 0; i < FAKE_LIFI_MAX_LINKS; i++) {
        registered_links[i].transmitter = NULL;
        registered_links[i].receiver = NULL;
    }
}

void Fake_LiFi_Link_Register(LiFi_Transmitter_t *transmitter, LiFi_Receiver_t *receiver) 
{
    for (uint8_t i = 0; i < FAKE_LIFI_MAX_LINKS; i++) {
        if (registered_links[i].transmitter == NULL) {
            registered_links[i].transmitter = transmitter;
            registered_links[i].receiver = receiver;
            return;
        }
    }

    TEST_FAIL_MESSAGE("fake LiFi link registry is full");
}

static Fake_LiFi_Link_t *find_link_by_transmitter(LiFi_Transmitter_t *transmitter) 
{
    for (uint8_t i = 0; i < FAKE_LIFI_MAX_LINKS; i++) {
        if (
            registered_links[i].transmitter != NULL &&
            registered_links[i].transmitter == transmitter
        ) {
            return &registered_links[i];
        }
    }

    return NULL;
}

void LiFi_Transmitter_TransmitBuffer(LiFi_Transmitter_t *transmitter, const uint8_t *buffer, uint8_t length) 
{
    Fake_LiFi_Link_t *link = find_link_by_transmitter(transmitter);

    TEST_ASSERT_NOT_NULL_MESSAGE(link, "receiver link is not found");
    TEST_ASSERT_NOT_NULL_MESSAGE(link->receiver, "receiver is not connected");

    for (uint8_t i = 1; i < length; i++) { // skip preambule
        link->receiver->rx_byte = buffer[i];

        if (link->receiver->on_byte_received != NULL && link->receiver->on_byte_received_callback_context != NULL) {
            link->receiver->on_byte_received(link->receiver->on_byte_received_callback_context);
        }
    }

    if (link->transmitter->on_buffer_transmitted != NULL && link->transmitter->on_buffer_transmitted_callback_context != NULL) {
        link->transmitter->on_buffer_transmitted(link->transmitter->on_buffer_transmitted_callback_context);
    }
}

void LiFi_Transmitter_ToConfirmationMode(LiFi_Transmitter_t *transmitter)
{
    
}

void LiFi_Receiver_ReadBuffer(LiFi_Receiver_t *receiver)
{
    (void)receiver;
}
