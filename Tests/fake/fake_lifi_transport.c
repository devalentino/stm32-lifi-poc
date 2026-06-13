#include "fake_lifi_transport.h"

#include <stdint.h>
#include <string.h>

#include "unity.h"

#define FAKE_LIFI_MAX_LINKS 5
static Fake_LiFi_Link_t registered_links[FAKE_LIFI_MAX_LINKS];

void Fake_LiFi_Link_Reset(void) {
  for (uint8_t i = 0; i < FAKE_LIFI_MAX_LINKS; i++) {
    registered_links[i].transmitter = NULL;
    registered_links[i].receiver = NULL;
  }
}

void Fake_LiFi_Link_Register(LiFi_Transmitter_t *transmitter, LiFi_Receiver_t *receiver) {
  for (uint8_t i = 0; i < FAKE_LIFI_MAX_LINKS; i++) {
    if (registered_links[i].transmitter == NULL) {
      registered_links[i].transmitter = transmitter;
      registered_links[i].receiver = receiver;
      return;
    }
  }

  TEST_FAIL_MESSAGE("fake LiFi link registry is full");
}

static Fake_LiFi_Link_t *find_link_by_transmitter(LiFi_Transmitter_t *transmitter) {
  for (uint8_t i = 0; i < FAKE_LIFI_MAX_LINKS; i++) {
    if (registered_links[i].transmitter != NULL && registered_links[i].transmitter == transmitter) {
      return &registered_links[i];
    }
  }

  return NULL;
}

void Fake_LiFi_Transmit(Fake_LiFi_Link_t *link) {
  LiFi_Transmitter_t *transmitter = link->transmitter;

  uint8_t tx_total_bytes = transmitter->tx_total_bytes;
  for (uint8_t i = transmitter->current_tx_byte_index; i < tx_total_bytes; i++) {
    link->receiver->rx_byte = transmitter->tx_buffer[transmitter->current_tx_byte_index++];

    if (link->receiver->on_byte_received != NULL &&
        link->receiver->on_byte_received_callback_context != NULL) {
      link->receiver->on_byte_received(link->receiver->on_byte_received_callback_context);
    }
  }

  transmitter->is_busy = false;

  if (transmitter->on_buffer_transmitted != NULL &&
      transmitter->on_buffer_transmitted_callback_context != NULL) {
    transmitter->on_buffer_transmitted(transmitter->on_buffer_transmitted_callback_context);
  }
}

void Fake_LiFi_RunUntilIdle(void) {
  Fake_LiFi_Link_t links_to_process[FAKE_LIFI_MAX_LINKS] = {0};

  uint8_t links_to_process_count = 0;
  for (uint8_t i = 0; i < FAKE_LIFI_MAX_LINKS; i++) {
    if (registered_links[i].transmitter != NULL && registered_links[i].transmitter->is_busy &&
        (registered_links[i].transmitter->current_tx_byte_index <
         registered_links[i].transmitter->tx_total_bytes)) {
      links_to_process[links_to_process_count++] = registered_links[i];
    }
  }

  for (uint8_t i = 0; i < FAKE_LIFI_MAX_LINKS; i++) {
    if (links_to_process[i].transmitter != NULL) {
      Fake_LiFi_Transmit(&links_to_process[i]);
    }
  }
}

void Fake_LiFi_Transmitter_TransmitBuffer_Callback(LiFi_Transmitter_t *transmitter,
                                                   const uint8_t *buffer, uint8_t length) {
  Fake_LiFi_Link_t *link = find_link_by_transmitter(transmitter);

  TEST_ASSERT_NOT_NULL_MESSAGE(link, "receiver link is not found");
  TEST_ASSERT_NOT_NULL_MESSAGE(link->receiver, "receiver is not connected");
  TEST_ASSERT_TRUE_MESSAGE(length <= LIFI_TX_BUFFER_SIZE, "fake transmit buffer is too small");
  TEST_ASSERT_FALSE_MESSAGE(transmitter->is_busy, "fake transmitter already has pending tx");

  memcpy(transmitter->tx_buffer, buffer, length);
  transmitter->tx_total_bytes = length;
  transmitter->current_tx_byte_index = 1; // skip preambule, matching physical receiver behavior
  transmitter->is_busy = true;
}

void LiFi_Receiver_ReadBuffer(LiFi_Receiver_t *receiver) {
  (void)receiver;
}
