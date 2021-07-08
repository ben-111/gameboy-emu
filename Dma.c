#include "Bus.h"
#include "Dma.h"

#define DMA_REG 0xFF46

// Static variables

// DMA register
static uint8_t DMA = 0x00;

static uint16_t RA = 0x0000; // Read address
static uint16_t WA = 0x0000; // Write address

void dma_clock()
{
	dma_write(WA++, dma_read(RA++));
}

void dma_reset()
{
	RA = 0x0000;
	WA = 0xFE00;
	DMA = 0x00;
}

void dma_init()
{
	RA = DMA << 8;
	WA = 0xFE00;
}

uint8_t dma_read(uint16_t addr)
{
	if (addr >= 0x0000 && addr <= 0xF19F)
		return bus_read(addr, DEV_DMA);
	else
		return 0xFF;
}

uint8_t dma_write(uint16_t addr, uint8_t data)
{
	if (addr >= 0xFE00 && addr <= 0xFE9F)
		return bus_write(addr, data, DEV_DMA);
	else
		return 0;
}

uint8_t dma_register_write(uint16_t addr, uint8_t data)
{
	if (addr == DMA_REG)
	{
		DMA = data;
		dma_init();
	}
	return 0;
}

uint8_t dma_register_read(uint16_t addr)
{
	if (addr == DMA_REG)
		return DMA;
	else
		return 0xFF;
}
