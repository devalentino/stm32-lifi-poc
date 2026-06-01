#include "stm32f4xx_hal.h"
#include "lifi_transmitter.h"

void SystemClock_Config(void);
void Error_Handler(void);
void Timer2_Init(void);

TIM_HandleTypeDef transmitter_timer;
LiFi_Transmitter_t transmitter;


void HAL_Hardware_Factory_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_TIM2_CLK_ENABLE();

  transmitter_timer.Instance = TIM2;
  transmitter_timer.Init.Prescaler = 0;
  transmitter_timer.Init.CounterMode = TIM_COUNTERMODE_UP;
  transmitter_timer.Init.Period = 4000 - 1;
  transmitter_timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  HAL_TIM_Base_Init(&transmitter_timer);

  HAL_NVIC_SetPriority(TIM2_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);
}


int main(void)
{
  HAL_Init();
  SystemClock_Config();
  HAL_Hardware_Factory_Init();

  LiFi_Transmitter_Init(&transmitter, &transmitter_timer, GPIOA, GPIO_PIN_5);

  while (1)
    {
      if (!LiFi_Transmitter_IsBusy(&transmitter))
      {
        LiFi_Transmitter_SendByte(&transmitter, 'S');
      }
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

void TIM2_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&transmitter_timer);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == transmitter_timer.Instance)
  {
	  LiFi_Transmitter_TimerCallback(&transmitter);
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
