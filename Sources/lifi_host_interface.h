#ifndef LIFI_HOST_INTERFACE_H
#define LIFI_HOST_INTERFACE_H

#include <stdint.h>
#include "stm32f4xx_hal_uart.h"

typedef void (*LiFi_HostInterface_BufferReceivedCallback)(
    void *on_buffer_received_callback_context);

typedef enum {
    HOST_INTERFACE_IDLE = 0x00,
    HOST_INTERFACE_RECEIVE = 0x01,
    HOST_INTERFACE_TRANSMIT = 0x02
} LiFi_HostInterface_State_t;

typedef struct {
  UART_HandleTypeDef *huart;

  LiFi_HostInterface_State_t state;

  uint8_t *tx_buffer;
  uint8_t tx_buffer_length;
  uint8_t tx_bytes_processed;

  uint8_t *rx_buffer;
  uint8_t rx_buffer_length;
  uint8_t rx_bytes_received;

  LiFi_HostInterface_BufferReceivedCallback on_buffer_received_callback;
  void *on_buffer_received_callback_context;
} LiFi_HostInterface_t;

void LiFi_HostInterface_Init(LiFi_HostInterface_t *host_interface, UART_HandleTypeDef *huart);

void LiFi_HostInterface_onReceiveSuccessfulCallback(LiFi_HostInterface_t *host_interface, uint8_t length);

#endif