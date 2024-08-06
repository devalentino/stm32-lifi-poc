#include "transmitter.h"


void setupTransmitter() {
	// Enable clock on GPIOA
	RCC->AHB1ENR |= (1U<<0);

	// setup transmitter, PA0
	GPIOA->MODER |= (1U<<0);
	GPIOA->MODER &= ~(1U<<1);
}

void transmitZero() {
	GPIOA->BSRR = 1U<<16;
}

void transmitOne() {
	GPIOA->BSRR = 1U<<0;
}
