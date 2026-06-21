#ifndef STM32F4XX_HAL_H
#define STM32F4XX_HAL_H

#include <stdint.h>

typedef struct { uint32_t unused; } TIM_HandleTypeDef;
typedef struct { uint32_t unused; } DMA_HandleTypeDef;
typedef struct {
    DMA_HandleTypeDef *hdmatx;
    DMA_HandleTypeDef *hdmarx;
} UART_HandleTypeDef;
typedef struct { uint32_t unused; } GPIO_TypeDef;

#endif
