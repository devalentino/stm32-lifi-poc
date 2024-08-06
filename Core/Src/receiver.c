#include <stdint.h>
#include <stdio.h>
#include "receiver.h"


void setupReceiver() {
	// Configure PA1 port as ADC channel. Should be in analog mode
	RCC->AHB1ENR |= (1U<<0);

	GPIOA->MODER |= (1U<<2);
	GPIOA->MODER |= (1U<<3);

	// Configure ADC
	RCC->APB2ENR |= (1U<<8);

	ADC1->SQR3 = (1U<<0);
	ADC1->SQR1 = 0;

	// Enable ADC
	ADC1->CR2 |= (1U<<0);
}

void startReceiver() {
	ADC1->CR2 |= (1U<<30);
}

uint32_t receive() {
	while (!(ADC1->SR & (1U<<1))) {
		printf("%d", 1);
	}

	return (uint32_t) ADC1->DR;
}
