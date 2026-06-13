#include "lifi_transmitter.h"

#include <stdint.h>

#include "lifi_const.h"
#include "stm32f4xx_hal.h"

static void copy_to_tx_buffer(uint8_t *tx_buffer, const uint8_t *buffer, uint8_t length) {
  for (uint8_t i = 0; i < length; i++) {
    tx_buffer[i] = buffer[i];
  }
}

static void byte_to_manchester(uint8_t *manchester_buffer, uint8_t payload) {
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t bit = (payload >> (7 - i)) & 0x01;
    if (bit == 1) {
      manchester_buffer[i * 2] = 1;
      manchester_buffer[i * 2 + 1] = 0;
    } else {
      manchester_buffer[i * 2] = 0;
      manchester_buffer[i * 2 + 1] = 1;
    }
  }
}

void LiFi_Transmitter_ToTransmitMode(LiFi_Transmitter_t *transmitter) {
  __HAL_TIM_SET_PRESCALER(transmitter->htim, LIFI_TRANSMIT_TIMER_PRESCALER);
  transmitter->htim->Instance->EGR = TIM_EGR_UG;
}

void LiFi_Transmitter_ToConfirmationMode(LiFi_Transmitter_t *transmitter) {
  __HAL_TIM_SET_PRESCALER(transmitter->htim, LIFI_TRANSMIT_CONFIRM_TIMER_PRESCALER);
  transmitter->htim->Instance->EGR = TIM_EGR_UG;
}

void LiFi_Transmitter_Init(LiFi_Transmitter_t *transmitter, TIM_HandleTypeDef *htim,
                           GPIO_TypeDef *port, uint16_t pin) {
  transmitter->htim = htim;
  transmitter->gpio_port = port;
  transmitter->gpio_pin = pin;
  transmitter->current_half_bit_index = -1;
  transmitter->is_busy = false;
  transmitter->on_buffer_transmitted = NULL;
  transmitter->on_buffer_transmitted_callback_context = NULL;
}

void LiFi_Transmitter_TransmitBuffer(LiFi_Transmitter_t *transmitter, const uint8_t *buffer,
                                     uint8_t length) {
  if (transmitter->is_busy)
    return;
  if (length > LIFI_TX_BUFFER_SIZE || length == 0)
    return;

  transmitter->is_busy = true;
  LiFi_Transmitter_ToTransmitMode(transmitter);

  transmitter->tx_total_bytes = length;
  transmitter->current_tx_byte_index = 0;
  copy_to_tx_buffer(transmitter->tx_buffer, buffer, length);

  byte_to_manchester(transmitter->manchester_buffer,
                     transmitter->tx_buffer[transmitter->current_tx_byte_index]);
  transmitter->current_half_bit_index = 0;

  HAL_TIM_Base_Start_IT(transmitter->htim);
}

void LiFi_Transmitter_TimerCallback(LiFi_Transmitter_t *transmitter) {
  // buffer is fully sent, stop timer, disable laser and exit
  if (transmitter->current_half_bit_index == 16 &&
      transmitter->current_tx_byte_index == transmitter->tx_total_bytes) {
    HAL_TIM_Base_Stop_IT(transmitter->htim);
    HAL_GPIO_WritePin(transmitter->gpio_port, transmitter->gpio_pin, GPIO_PIN_RESET);
    transmitter->current_half_bit_index = -1;
    transmitter->is_busy = false;
    if (transmitter->on_buffer_transmitted != NULL &&
        transmitter->on_buffer_transmitted_callback_context != NULL) {
      transmitter->on_buffer_transmitted(transmitter->on_buffer_transmitted_callback_context);
    }
    return;
  }

  // if byte is transmitted, but buffer is not transmitted yet, configure next
  // byte transmission
  if (transmitter->current_half_bit_index == 16 &&
      transmitter->current_tx_byte_index < transmitter->tx_total_bytes) {
    transmitter->current_half_bit_index = 0;
    transmitter->current_tx_byte_index++;
    byte_to_manchester(transmitter->manchester_buffer,
                       transmitter->tx_buffer[transmitter->current_tx_byte_index]);
  }

  // continue transmitting byte
  if (transmitter->current_half_bit_index >= 0 && transmitter->current_half_bit_index < 16) {
    uint8_t state = transmitter->manchester_buffer[transmitter->current_half_bit_index];
    HAL_GPIO_WritePin(transmitter->gpio_port, transmitter->gpio_pin,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    transmitter->current_half_bit_index++;
    return;
  }
}