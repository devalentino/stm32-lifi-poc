#include "stm32f4xx_hal.h"
#include "lifi_transmitter.h"
#include "lifi_receiver.h"  
#include "lifi_protocol.h"
#include <string.h>

void SystemClock_Config(void);
void Error_Handler(void);
void HAL_Hardware_Factory_Init(void);

TIM_HandleTypeDef transmitter_timer;
TIM_HandleTypeDef receiver_timer;

LiFi_Transmitter_t transmitter;
LiFi_Receiver_t receiver;
LiFi_Socket_t socket;


int main(void)
{
  HAL_Init();
  SystemClock_Config();
  HAL_Hardware_Factory_Init();

  LiFi_Transmitter_Init(&transmitter, &transmitter_timer, GPIOA, GPIO_PIN_5);
  LiFi_Receiver_Init(&receiver, &receiver_timer, GPIOA, GPIO_PIN_1);
  LiFi_Socket_Init(&socket, &transmitter, &receiver);

  char content[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat";
  char read_buffer[256] = {0};

  LiFi_Socket_Send(&socket, (uint8_t *) content, strlen(content));
  LiFi_Socket_Read(&socket, (uint8_t *) read_buffer);

  while (1) {
    HAL_Delay(1000);
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_Hardware_Factory_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM2_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();
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
	  transmitter_timer.Init.Prescaler = 0;
	  transmitter_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
	  transmitter_timer.Init.Period = 4000 - 1;
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
	  receiver_timer.Init.Prescaler = 16 - 1;
	  receiver_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
	  receiver_timer.Init.Period = 0xFFFF;
	  receiver_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	  HAL_TIM_Base_Init(&receiver_timer);
	  HAL_TIM_Base_Start(&receiver_timer);

	  HAL_NVIC_SetPriority(EXTI1_IRQn, 3, 0);
	  HAL_NVIC_EnableIRQ(EXTI1_IRQn);
  }
}

void TIM2_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&transmitter_timer);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    LiFi_Transmitter_TimerCallback(&transmitter);
  }
}

void EXTI1_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_1)
  {
    LiFi_Receiver_GPIO_Callback(&receiver);
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
