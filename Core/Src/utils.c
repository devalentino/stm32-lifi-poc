#include "utils.h"


void setupUtils() {
	// enable GPIOA and GPIOC
	RCC->AHB1ENR |= (1U<<0);
	RCC->AHB1ENR |= (1U<<2);

	// setup LED indicator, PA5
	GPIOA->MODER |= (1U<<10);
	GPIOA->MODER &= ~(1U<<11);

	// setup button, PC13
	GPIOC->MODER &= ~(1U<<26);
	GPIOC->MODER &= ~(1U<<27);
}

bool isButtonPressed() {
	return (GPIOC->IDR & (1U<<13)) == 0;
}

void enableLedIndicator() {
	GPIOA->BSRR |= (1U<<5);
}

void disableLedIndicator() {
	GPIOA->BSRR |= (1U<<21);
}
