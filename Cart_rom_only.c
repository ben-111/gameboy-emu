#include "Cart_rom_only.h"

// Initialize static variables
static uint8_t* rom = NULL;
static uint8_t* ram = NULL;
static uint8_t ro_romsize = 0x00;
static uint8_t ro_ramsize = 0x00;

// Initialize ROM and RAM
uint8_t cart_rom_only_init(uint8_t romsize, uint8_t ramsize, uint8_t* temp_rom)
{
	ro_romsize = romsize;
	ro_ramsize = ramsize;
	rom = (uint8_t*)calloc(32 * 1024, sizeof(uint8_t));
	if (rom == NULL)
		return 1;
	int res = memcpy_s(rom, 32 * 1024, temp_rom, 32 * 1024);
	if (res != 0)
	{
		free(rom);
		return 1;
	}
	if (ramsize)
	{
		ram = (uint8_t*)calloc(ramsize - 1 ? 8 * 1024 : 2 * 1024, sizeof(uint8_t));
		if (ram == NULL)
		{
			free(rom);
			return 1;
		}
	}
	else
		ram = NULL;
	return 0;
}

void cart_rom_only_free()
{
	if (rom != NULL)
	{
		free(rom);
		rom = NULL;
	}
	if (ram != NULL)
	{
		free(ram);
		ram = NULL;
	}
}

void cart_rom_only_reset()
{
}

uint8_t cart_rom_only_write(uint16_t addr, uint8_t data)
{
	if (addr >= 0x000 && addr <= 0x7FFF)
	{
		// Do nothing
	}
	else if (addr >= 0xA000 && addr <= 0xBFFF)
	{
		if (ram != NULL)
		{
			if (!(ro_ramsize == CART_RAM_2K && addr >= 0xA800))
			{
				ram[addr - 0xA000] = data;
			}
		}
	}
	return 0;
}

uint8_t cart_rom_only_read(uint16_t addr)
{
	if (addr >= 0x000 && addr <= 0x7FFF)
	{
		return rom[addr];
	}
	else if (addr >= 0xA000 && addr <= 0xBFFF)
	{
		if (ram != NULL)
		{
			if (!(ro_ramsize == CART_RAM_2K && addr >= 0xA800))
			{
				return ram[addr - 0xA000];
			}	
		}
	}
	return 0xFF;
}

uint16_t cart_rom_only_compute_global_checksum()
{
	uint16_t global_checksum = 0;
	for (int i = 0; i < 32 * 1024; i++)
	{
		if (i != 0x14E && i != 0x014F)
			global_checksum += rom[i];
	}
	return global_checksum;
}
