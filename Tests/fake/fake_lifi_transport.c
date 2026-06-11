#include "unity.h"
#include <stdint.h>
#include <string.h>
#include "fake_lifi_transport.h"

#define FAKE_LIFI_MAX_LINKS 5
static Fake_LiFi_Link_t registered_links[FAKE_LIFI_MAX_LINKS];

void Fake_LiFi_Link_Reset(void)
{
    for (uint8_t i = 0; i < FAKE_LIFI_MAX_LINKS; i++) {
        registered_links[i].transmitter = NULL;
        registered_links[i].receiver = NULL;
        registered_links[i].pending_length = 0;
        registered_links[i].pending_index = 0;
        registered_links[i].has_pending_tx = false;
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

void Fake_LiFi_Transmit(Fake_LiFi_Link_t *link)
{
    if (link->pending_index < link->pending_length) {
        link->receiver->rx_byte = link->pending_buffer[link->pending_index++];

        if (link->receiver->on_byte_received != NULL && link->receiver->on_byte_received_callback_context != NULL) {
            link->receiver->on_byte_received(link->receiver->on_byte_received_callback_context);
        }

        return;
    }

    link->has_pending_tx = false;
    link->transmitter->is_busy = false;

    if (link->transmitter->on_buffer_transmitted != NULL && link->transmitter->on_buffer_transmitted_callback_context != NULL) {
        link->transmitter->on_buffer_transmitted(link->transmitter->on_buffer_transmitted_callback_context);
    }
}

void Fake_LiFi_RunUntilIdle(void)
{
    for (uint8_t i = 0; i < FAKE_LIFI_MAX_LINKS; i++) {
        while (registered_links[i].has_pending_tx) {
            Fake_LiFi_Transmit(&registered_links[i]);
        }
    }
}

void Fake_LiFi_Transmitter_TransmitBuffer_Callback(LiFi_Transmitter_t *transmitter, const uint8_t *buffer, uint8_t length)
{
    Fake_LiFi_Link_t *link = find_link_by_transmitter(transmitter);

    TEST_ASSERT_NOT_NULL_MESSAGE(link, "receiver link is not found");
    TEST_ASSERT_NOT_NULL_MESSAGE(link->receiver, "receiver is not connected");
    TEST_ASSERT_TRUE_MESSAGE(length <= LIFI_TX_BUFFER_SIZE, "fake transmit buffer is too small");
    TEST_ASSERT_FALSE_MESSAGE(link->has_pending_tx, "fake link already has pending tx");

    memcpy(link->pending_buffer, buffer, length);
    link->pending_length = length;
    link->pending_index = 1; // skip preambule, matching physical receiver behavior
    link->has_pending_tx = true;
    transmitter->is_busy = true;
}

void LiFi_Receiver_ReadBuffer(LiFi_Receiver_t *receiver)
{
    (void)receiver;
}
