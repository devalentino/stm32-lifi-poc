#ifndef LIFI_TRANSMITTER_H
#define LIFI_TRANSMITTER_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#define LIFI_TX_BUFFER_SIZE 40

typedef void (*LiFi_Transmitter_BufferTransmittedCallback)(
    void *on_buffer_transmitted_callback_context);

typedef void (*LIFi_Transmitter_TimeoutCallback)(void *on_timeout_callback_context);

typedef struct {
  TIM_HandleTypeDef *htim;
  GPIO_TypeDef *gpio_port;
  uint16_t gpio_pin;

  uint8_t tx_buffer[LIFI_TX_BUFFER_SIZE];
  uint8_t tx_total_bytes;
  volatile uint8_t current_tx_byte_index;

  uint8_t manchester_buffer[16];
  volatile int16_t current_half_bit_index;
  volatile bool is_busy;

  LiFi_Transmitter_BufferTransmittedCallback on_buffer_transmitted;
  void *on_buffer_transmitted_callback_context;

  LIFi_Transmitter_TimeoutCallback on_timeout_callback;
  void *on_timeout_callback_context;
} LiFi_Transmitter_t;

void LiFi_Transmitter_Init(LiFi_Transmitter_t *transmitter, TIM_HandleTypeDef *htim,
                           GPIO_TypeDef *port, uint16_t pin);

void LiFi_Transmitter_TransmitBuffer(LiFi_Transmitter_t *transmitter, const uint8_t *buffer,
                                     uint8_t length);

void LiFi_Transmitter_TimerCallback(LiFi_Transmitter_t *transmitter);

void LiFi_Transmitter_ToTransmitMode(LiFi_Transmitter_t *transmitter);

void LiFi_Transmitter_ToConfirmationMode(LiFi_Transmitter_t *transmitter);

#endif
