#ifndef LIFI_RECEIVER_H
#define LIFI_RECEIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

typedef struct {
    TIM_HandleTypeDef *htim;
    GPIO_TypeDef      *gpio_port;
    uint16_t           gpio_pin;

    uint8_t            rx_byte;
    uint8_t            bit_count;
    bool               is_first_half;
    volatile bool      has_new_data;
} LiFi_Receiver_t;

void LiFi_Receiver_Init(LiFi_Receiver_t *receiver, TIM_HandleTypeDef *htim, GPIO_TypeDef *port, uint16_t pin);

bool LiFi_Receiver_ReadBuffer(LiFi_Receiver_t *receiver, uint8_t *out_byte);

void LiFi_Receiver_GPIO_Callback(LiFi_Receiver_t *receiver);

#endif
