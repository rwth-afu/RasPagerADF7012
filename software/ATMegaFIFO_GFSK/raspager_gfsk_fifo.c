/*
 * Copyright (C) 2017 Amateurfunkgruppe der RWTH Aachen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

// AT Mega Frequency
#define F_CPU 8000000

// TRM_DATA -> This pin shifts out the data to the transmitter
// ATMega: PB1, ADF7012: TxData
#define TRM_DATA_PORT      PORTB
#define TRM_DATA_DDR       DDRB
#define TRM_DATA_BIT       1

// TRM_CLOCK -> This pin is the data clock from the ADF7012 to the ATMega
// ATMega: PD2 (INT0), ADF7012: TXCLK
#define TRM_CLOCK_PORT      PORTD
#define TRM_CLOCK_DDR       DDRD
#define TRM_CLOCK_BIT       2

// RPI_HANDSHAKE -> This pin tells the RPI, when it is able to receive data PD3
#define RPI_HANDSHAKE_PORT PORTD
#define RPI_HANDSHAKE_DDR  DDRD
#define RPI_HANDSHAKE_BIT  3

// RPI_SHIFTING -> This pin tells the RPI, if it is still shifting out data PD4
#define RPI_SHIFTING_PORT  PORTD
#define RPI_SHIFTING_DDR   DDRD
#define RPI_SHIFTING_BIT   4

// RPI_DATA -> This pin receives the data from the RPI PD0
#define RPI_DATA_PORT      PORTD
#define RPI_DATA_PIN       PIND
#define RPI_DATA_BIT       0

// RPI_CLOCK -> This pin receives the data clock from the RPI PD1
#define RPI_CLOCK_PORT     PORTD
#define RPI_CLOCK_PIN      PIND
#define RPI_CLOCK_BIT      1

// For GFSK: 


// ############################################################################
// #                              Preparations                                #
// ############################################################################



// Avr-libc include files
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// Bit Manipulation
#define SET_BIT(var, bitnum)   (var) |=  (1<<(bitnum))
#define CLR_BIT(var, bitnum)   (var) &= ~(1<<(bitnum))
#define TOG_BIT(var, bitnum)   (var) ^=  (1<<(bitnum))
#define GET_BIT(var, bitnum)  (((var)>>(bitnum)) & 0x1)

// IO Defines
#define INIT_OUTPUT(signal)  {SET_BIT(signal##_DDR,  signal##_BIT);}
#define SET_OUTPUT(signal)   {SET_BIT(signal##_PORT, signal##_BIT);}
#define CLR_OUTPUT(signal)   {CLR_BIT(signal##_PORT, signal##_BIT);}
#define IS_INPUT(signal)     (GET_BIT(signal##_PIN,  signal##_BIT))



// ############################################################################
// #                                  FiFo                                    #
// ############################################################################



#define FIFO_Init(name, type, size)          \
static uint16_t FIFOBUF_##name##_push  = 0; \
static uint16_t FIFOBUF_##name##_pop   = 0; \
static uint16_t FIFOBUF_##name##_count = 0; \
static type FIFOBUF_##name##_data[size];

#define FIFO_Clear(name)      \
FIFOBUF_##name##_push  = 0; \
FIFOBUF_##name##_pop   = 0; \
FIFOBUF_##name##_count = 0;

#define FIFO_Empty(name) \
(!FIFOBUF_##name##_count)

#define FIFO_Full(name) \
(FIFOBUF_##name##_count == sizeof(FIFOBUF_##name##_data))

#define FIFO_Push(name, value)                          \
FIFOBUF_##name##_data[FIFOBUF_##name##_push] = value; \
FIFOBUF_##name##_count++;                             \
FIFOBUF_##name##_push++;                              \
if(FIFOBUF_##name##_push >= sizeof(FIFOBUF_##name##_data)) FIFOBUF_##name##_push = 0;

#define FIFO_Pop(name, var)                          \
var = FIFOBUF_##name##_data[FIFOBUF_##name##_pop]; \
FIFOBUF_##name##_count--;                          \
FIFOBUF_##name##_pop++;                            \
if(FIFOBUF_##name##_pop >= sizeof(FIFOBUF_##name##_data)) FIFOBUF_##name##_pop = 0;

FIFO_Init(buffer, uint8_t, 512);

// INT 1 Service Routine. Executed on falling edge. Set new bit on each execution.
ISR(INT1_vect) {

	// Handle shifting out of data
	if (!FIFO_Empty(buffer)) {     // Data in buffer?
		uint8_t databit;
		FIFO_Pop(buffer, databit); // Get bit from FIFO
		if (databit) {                // Databit is "1"?
			SET_OUTPUT(TRM_DATA);    // Set data output high
		} else {                       // Databit is "0"?
			CLR_OUTPUT(TRM_DATA);    // Set data output low
		}
		SET_OUTPUT(RPI_SHIFTING);  // Tell RPI that currently data is shifted out to the transmitter
	} else {                         // No data in buffer anymore?
		CLR_OUTPUT(TRM_DATA);      // Set data output low
		CLR_OUTPUT(RPI_SHIFTING);  // Tell RPI that no data is shifted out at the moment
	}
}

void init() {
	// Enable outputs
	INIT_OUTPUT(TRM_DATA);
	INIT_OUTPUT(RPI_HANDSHAKE);
	INIT_OUTPUT(RPI_SHIFTING);

	// Set up INT1_vect
	
	// Set PD2 = INT0 as Input
	DDRD &= ~(1 << DDD2);
    
	// Enable Falling edge activation
	MCUCR = (MCUCR & 0b11111100) | (1 << ISC01);
	
	// Enable INT0 interrupt
	GICR |= (1 << INT0); 
	
	// Enable interrupts
	sei();

}

int main() {
	init();
	
	while(1)
	{
		// Handle receiving of data
		if (FIFO_Full(buffer)) {                      // Buffer full?
			CLR_OUTPUT(RPI_HANDSHAKE);               // Set handshake output to low -> Not able to receive anymore
		} else {                                       // Buffer not full yet?
			static uint8_t last_clk_state;
			uint8_t current_clk_state;
			uint8_t databit;

			current_clk_state = IS_INPUT(RPI_CLOCK); // Get current clock state
			databit           = IS_INPUT(RPI_DATA);  // Get data bit value

			// Clock edge from high to low?
			if ((last_clk_state == 1) && (current_clk_state == 0)) {
				cli();                                 // Disable interrupt
				FIFO_Push(buffer, databit);            // Push data to buffer
				sei();                                 // Enable interrupt
			}

			last_clk_state = current_clk_state;      // Remember clockstate for next cycle

			SET_OUTPUT(RPI_HANDSHAKE);               // Set handshake output to high -> Able to receive data
		}
	}
	return 0;
} 

