#include "lifi_const.h"
#include "lifi_protocol.h"
#include "lifi_receiver.h"
#include "lifi_transmitter.h"
#include "stm32f4xx_hal.h"
#include <string.h>

void SystemClock_Config(void);
void Error_Handler(void);
void HAL_Hardware_Factory_Init(void);

TIM_HandleTypeDef transmitter_timer;
TIM_HandleTypeDef receiver_timer;
UART_HandleTypeDef uart;

LiFi_Transmitter_t transmitter;
LiFi_Receiver_t receiver;
LiFi_Socket_t socket;

static void on_error(LiFi_Socket_Error_t error, LiFi_Socket_t *socket) {
  __NOP() :
}

static void on_transmission_success(LiFi_Socket_t *socket) {
  __NOP();
}

static void on_receive_success(LiFi_Socket_t *socket) {
  __NOP();
}

int main(void) {
  HAL_Init();
  SystemClock_Config();

  while (1) {
    // TODO: initialize sockets and wait for messages
    HAL_Delay(1000);
  }
}

void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType =
      RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }
}

void HAL_Hardware_Factory_Init(void) {
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM2_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  // GPIOA transmitter setup
  {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

    transmitter_timer.Instance = TIM2;
    transmitter_timer.Init.Prescaler = LIFI_TRANSMIT_TIMER_PRESCALER;
    transmitter_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    transmitter_timer.Init.Period = LIFI_TRANSMIT_TIMER_PERIOD;
    transmitter_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&transmitter_timer);

    HAL_NVIC_SetPriority(TIM2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
  }

  // GPIOA receiver setup
  {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    receiver_timer.Instance = TIM3;
    receiver_timer.Init.Prescaler = LIFI_TRANSMIT_TIMER_PRESCALER;
    receiver_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    receiver_timer.Init.Period = 0xFFFF;
    receiver_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&receiver_timer);
    HAL_TIM_Base_Start(&receiver_timer);

    HAL_NVIC_SetPriority(EXTI1_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
  }

  // UART configuration
  {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  }
}

void TIM2_IRQHandler(void) {
  HAL_TIM_IRQHandler(&transmitter_timer);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM2) {
    LiFi_Transmitter_TimerCallback(&transmitter);
  } else if (htim->Instance == TIM3) {
    LiFi_Receiver_TimerCallback(&server_receiver);
  }
}

void EXTI1_IRQHandler(void) {
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == GPIO_PIN_1) {
    LiFi_Receiver_GPIO_Callback(&receiver);
  }
}

void SysTick_Handler(void) {
  HAL_IncTick();
}

void Error_Handler(void) {
  __disable_irq();
  while (1) {
  }
}
