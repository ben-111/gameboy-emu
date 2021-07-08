#include "Bus.h"
#include "Timer.h"

#define IF_ADDR 0xFF0F

// Static variables

// Divider register: FF04
static uint16_t DIV = 0x0000;
static uint16_t prev_DIV = 0x0000;

// Timer counter: FF05
static uint8_t TIMA = 0x00;

// Timer modulo: FF06
static uint8_t TMA = 0x00;

// Timer control: FF07
static uint8_t TAC = 0x00;

// Global variables
const uint16_t TIMER_FREQS[4] = {0x0200, 0x0008, 0x0020, 0x0080};
uint8_t counter = 0xFF;

void timer_reset() 
{
	DIV = 0x00;
	TIMA = 0x00;
	TMA = 0x00;
	TAC = 0x00;
}

// Registers:
// DIV register:
// Divider register
// Always counts at 16384 Hz, or CPU Clock / 256 (CPU Clock = 4 MHz)
// When written to, resets to 0.
//
// TIMA register:
// Timer counter
// Counts at a frequency specified by the TAC register, if enabled. When overflows, it resets to the value 
// in the TMA register, and requests an interrupt.
//
// TMA register:
// Contains the value that the TIMA register resets to.
//
// TAC register:
// Timer Control
// Bit  2 - Timer Enable
// Bits 1 - 0 - Input Clock Select
//		00: CPU Clock / 1024 (DMG, CGB:   4096 Hz)
//		01 : CPU Clock / 16   (DMG, CGB: 262144 Hz)
//		10 : CPU Clock / 64   (DMG, CGB:  65536 Hz)
//		11 : CPU Clock / 256  (DMG, CGB:  16384 Hz)
//
// 
void timer_clock()
{
	if (TAC & TAC_ENABLE)
	{
		
		uint16_t freq = TIMER_FREQS[TAC & 0x03];
		if (((DIV++ & freq) > 0) && ((DIV & freq) == 0))
		{
			if (TIMA++ == 0xFF)
				counter = 4;
		}
		if (counter > 0)
			counter--;
		if (TIMA == 0 && counter == 0)
		{
			// Reset TIMA
			TIMA = TMA;

			counter = 0xFF;

			// Request interrupt
			timer_write(IF_ADDR, timer_read(IF_ADDR) | INT_TIMER);
		}
	}
	else
		DIV++;
}

uint8_t timer_register_write(uint16_t addr, uint8_t data)
{
	switch (addr)
	{
	case 0xFF04:
	{
		uint16_t freq = TIMER_FREQS[TAC & 0x03];
		if (((DIV & freq) > 0) && (TAC & TAC_ENABLE))
			TIMA++;
		DIV = 0x0000;
		break;
	}
	case 0xFF05:
		if (TIMA == 0 && counter >= 0 && counter < 5)
			counter = 0xFF;
		TIMA = data;
		break;
	case 0xFF06:
		TMA = data;
		break;
	case 0xFF07:
	{
		uint16_t freq = TIMER_FREQS[TAC & 0x03];
		bool old = (TAC & TAC_ENABLE) && ((DIV & freq) > 0);
		freq = TIMER_FREQS[data & 0x03];
		bool new = (data & TAC_ENABLE) && ((DIV & freq) > 0);
		if (old && !new)
			TIMA++;
		TAC = data & 0x07;
		break;
	}
	}
	return 0;
}

uint8_t timer_register_read(uint16_t addr)
{
	switch (addr)
	{
		case 0xFF04:
			return DIV >> 8;
		case 0xFF05:
			return TIMA;
		case 0xFF06:
			return TMA;
		case 0xFF07:
			return (TAC & 0x07) | 0xF8;
		default:
			return 0xFF;
	}
}

uint8_t timer_read(uint16_t addr)
{
	return bus_read(addr, DEV_TIMER);
}

uint8_t timer_write(uint16_t addr, uint8_t data) 
{
	return bus_write(addr, data, DEV_TIMER);
}