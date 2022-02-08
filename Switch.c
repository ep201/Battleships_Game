// Switch.c
// This software to input from switches or buttons
// Runs on TM4C123
// Program written by: put your names here
// Date Created: 3/6/17 
// Last Modified: 1/14/21
// Lab number: 10
// Hardware connections
// Using PE1 for Pause/Play/Start, PE0 for Fire

#include <stdint.h>
#include "../inc/tm4c123gh6pm.h"

// Code files contain the actual implemenation for public functions
// this file also contains an private functions and private data

//initializes PE0, 1
void Switch_Init(void){
	SYSCTL_RCGCGPIO_R |= 0x10;
	__asm__{
		NOP
		NOP
		NOP
		NOP
	}
	GPIO_PORTE_DIR_R &= ~(0x03);
	GPIO_PORTE_DEN_R |= 0x03;
}

uint32_t FireIn(void){ //PE0
	uint32_t value = GPIO_PORTE_DATA_R & 0x01;
	while((GPIO_PORTE_DATA_R & 0x01) > 0){
	}
	return value;
	
}

uint32_t PlayIn(void){ //PE1
	uint32_t value = (GPIO_PORTE_DATA_R & 0x02)>>1;
	while(((GPIO_PORTE_DATA_R & 0x02)>>1) > 0){
	}
	return value;
	
}

