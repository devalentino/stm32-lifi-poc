#ifndef LIFI_TRANSMITTER_H
#define LIFI_TRANSMITTER_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

typedef struct {
    TIM_HandleTypeDef *htim;
    GPIO_TypeDef      *gpio_port;
    uint16_t           gpio_pin;

    uint8_t            manchester_buffer[16];
    volatile int16_t   current_half_bit_index;
    volatile bool      is_busy;
} LiFi_Transmitter_t;

void LiFi_Transmitter_Init(LiFi_Transmitter_t *transmitter, TIM_HandleTypeDef *htim, GPIO_TypeDef *port, uint16_t pin);

bool LiFi_Transmitter_SendByte(LiFi_Transmitter_t *transmitter, uint8_t data);

bool LiFi_Transmitter_IsBusy(LiFi_Transmitter_t *transmitter);

void LiFi_Transmitter_TimerCallback(LiFi_Transmitter_t *transmitter);

#endif
