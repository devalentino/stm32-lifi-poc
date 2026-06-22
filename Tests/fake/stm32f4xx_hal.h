#ifndef STM32F4XX_HAL_H
#define STM32F4XX_HAL_H

#include <stdint.h>

  #define UART_IT_TXE 0x00000001U

#define __HAL_UART_DISABLE_IT(__HANDLE__, __INTERRUPT__) ((void)(__HANDLE__), (void)(__INTERRUPT__))
#define __HAL_UART_ENABLE_IT(__HANDLE__, __INTERRUPT__) ((void)(__HANDLE__), (void)(__INTERRUPT__))

typedef struct { uint32_t unused; } TIM_HandleTypeDef;
typedef struct { uint32_t unused; } DMA_HandleTypeDef;

typedef struct {
  uint32_t DR;
} USART_TypeDef;
typedef struct {
  USART_TypeDef *Instance;
  DMA_HandleTypeDef *hdmatx;
  DMA_HandleTypeDef *hdmarx;
} UART_HandleTypeDef;
typedef struct { uint32_t unused; } GPIO_TypeDef;

#endif
