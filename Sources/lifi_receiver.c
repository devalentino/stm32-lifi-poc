#include "lifi_receiver.h"

#define T_SHORT_MIN  150
#define T_SHORT_MAX  350
#define T_LONG_MIN   400
#define T_LONG_MAX   600

void LiFi_Receiver_Init(LiFi_Receiver_t *receiver, TIM_HandleTypeDef *htim, GPIO_TypeDef *port, uint16_t pin)
{
    receiver->htim = htim;
    receiver->gpio_port = port;
    receiver->gpio_pin = pin;
    receiver->rx_byte = 0;
    receiver->bit_count = 0;
    receiver->is_first_half = true;
}

void LiFi_Receiver_ReadBuffer(LiFi_Receiver_t *receiver) {
    receiver->rx_byte = 0;
    receiver->bit_count = 0;
    receiver->is_first_half = true;
}

void LiFi_Receiver_GPIO_Callback(LiFi_Receiver_t *receiver)
{
    uint32_t time_elapsed = __HAL_TIM_GET_COUNTER(receiver->htim);
    __HAL_TIM_SET_COUNTER(receiver->htim, 0);

    uint8_t pin_state = HAL_GPIO_ReadPin(receiver->gpio_port, receiver->gpio_pin);
    
    if (time_elapsed > T_LONG_MAX) {
        receiver->bit_count = 0;
        receiver->rx_byte = 0;
        receiver->is_first_half = true;
        return; 
    }

    bool is_half_period = (time_elapsed >= T_SHORT_MIN && time_elapsed <= T_SHORT_MAX);
    bool is_full_period  = (time_elapsed >= T_LONG_MIN  && time_elapsed <= T_LONG_MAX);

    if (!is_half_period && !is_full_period) return;

    if ((is_half_period && receiver->is_first_half) || is_full_period) {
        uint8_t bit = (pin_state == GPIO_PIN_RESET) ? 1 : 0;
        
        receiver->rx_byte = (receiver->rx_byte << 1) | bit;
        receiver->bit_count++;
        receiver->is_first_half = false;
    } else {
        receiver->is_first_half = true;
    }

    if (receiver->bit_count >= 8) {
        receiver->bit_count = 0;

        if (receiver->on_byte_received != NULL && receiver->on_byte_received_callback_context != NULL) {
            receiver->on_byte_received(receiver->on_byte_received_callback_context);
        }
    }
}