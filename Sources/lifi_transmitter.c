#include "lifi_transmitter.h"

void LiFi_Transmitter_Init(LiFi_Transmitter_t *transmitter, TIM_HandleTypeDef *htim, GPIO_TypeDef *port, uint16_t pin)
{
    transmitter->htim = htim;
    transmitter->gpio_port = port;
    transmitter->gpio_pin = pin;
    transmitter->current_half_bit_index = -1;
    transmitter->is_busy = false;

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
}

bool LiFi_Transmitter_IsBusy(LiFi_Transmitter_t *transmitter)
{
    return transmitter->is_busy;
}

bool LiFi_Transmitter_SendByte(LiFi_Transmitter_t *transmitter, uint8_t data)
{
    if (transmitter->is_busy) return false;
    transmitter->is_busy = true;

    for (int i = 0; i < 8; i++)
    {
        uint8_t bit = (data >> (7 - i)) & 0x01;
        if (bit == 1) {
            transmitter->manchester_buffer[i * 2]     = 1;
            transmitter->manchester_buffer[i * 2 + 1] = 0;
        } else {
            transmitter->manchester_buffer[i * 2]     = 0;
            transmitter->manchester_buffer[i * 2 + 1] = 1;
        }
    }

    transmitter->current_half_bit_index = 0;

    HAL_TIM_Base_Start_IT(transmitter->htim);

    return true;
}

void LiFi_Transmitter_TimerCallback(LiFi_Transmitter_t *transmitter)
{
    if (transmitter->current_half_bit_index >= 0 && transmitter->current_half_bit_index < 16)
    {
        uint8_t state = transmitter->manchester_buffer[transmitter->current_half_bit_index];
        HAL_GPIO_WritePin(transmitter->gpio_port, transmitter->gpio_pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
        transmitter->current_half_bit_index++;
    }
    else
    {
        HAL_TIM_Base_Stop_IT(transmitter->htim);
        HAL_GPIO_WritePin(transmitter->gpio_port, transmitter->gpio_pin, GPIO_PIN_RESET);
        transmitter->current_half_bit_index = -1;
        transmitter->is_busy = false;
    }
}
