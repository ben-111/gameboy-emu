#include "Cart_mbc1.h"

// Memory map:
// 0000 - 3FFF: Normally contains ROM bank 00 (first 16KB), could have banks 20/40/60 in 
// mode 1 or 10/20/30 in mode 1 for 1MB multi-cart
// 4000 - 7FFF: ROM bank 01 - 7F, cannot have any bank that would
// be in when the banking register is 00, so it automatically maps it to 1
// bank higher. Up to 2MB ROM or 128 banks (125).
// A000 - BFFF: RAM bank 00 - 03, if any. Could be up to 32KB,
// where in 32KB mode it uses 4 banks.
//
// Control registers:
// 0000 - 1FFF: RAM enable. It is disabled by default.
// Any value with 0x0A in the lower 4 bits will enable it,
// anything else will disable it.
// 2000 - 3FFF: ROM bank number. Only the first max 5 bits are used,
// if they're not needed then even less bits are used. On larger
// carts which need more than 5 bits bank number, the second 
// banking register at 4000 - 5FFF is used to supply an additional
// 2 bits (Selected ROM bank = (Second bank << 5) + ROM bank).
// The ROM bank defaults to 01. 00 is automattically converted to 1 because 00 is 
// mapped to 0000 - 3FFF. On larger carts this also happens for
// banks 20h, 40h and 60h, and they are converted to 21h, 41h and 61h.
// To access these banks, you must enter mode 1, which remaps the 0000 - 3FFF
// range.
// 4000 - 5FFF: Second bank register. Depending on the cart,
// can be used for selecting the higher banks or selecting the RAM bank.
// 6000 - 7FFF: Mode select. Can be 00 or 01.
// Mode 0:
// In this mode RAM banking is disabled and the second banking register affects
// only the 0000 - 7FFF bank. Basically you get up to 8KB RAM, and up to 2MB ROM.
// Mode 1:
// According to the Pan Docs, mode 1 is different if you have a large ROM (>=1MB) or a large RAM (>8KB). 
// They do not say what happens in the large ROM and large RAM case, and whether it's even possible.
// In a large ROM cartridge, mode 1 is the same as mode 0, but it also remaps the 0000 - 3FFF area according to the
// second banking register. meaning, in mode 1 you can change the normally bank 00 to bank 20h/40h/60h.
// So, when selecting banks 00 - 1F, the first bank will be 00. When selecting banks 20 - 3F, the first bank
// will be 20. When selecting banks 40 - 5F, the first bank will be 40. And when selecting banks 60 - 7F,
// the first bank will be 60.
// In a large RAM cartridge, mode 1 makes the second banking register to a RAM bank selector, and only the first
// banking register is considered when selecting a ROM bank, therefore making only the first 512KB of ROM banks accessible.
// Since the Pan Docs did not specify the behavior in large ROM and RAM mod, I will make a guess that it is treated as
// the large RAM mode, and enable RAM banking.
// 
// Update! According to the complete GameBoy technical reference, it is a lot simpler than I thought.
// Mode 0:
// ROM banking always works the same, "bank 00" is always bank 00.
// RAM banking is disabled.
// Mode 1:
// ROM banking always works the same, "bank 00" is the second bank register << 5.
// RAM banking is enabled.

// TODO: Add support for MBC1M (multi-game compilation) carts, which use a slightly different mapping

#define RAMBANK_SIZE (8 * 1024)
#define ROMBANK_SIZE (16 * 1024)

// Static variables
// Consts
static uint8_t mbc1_romsize_code = 0x00;
static uint8_t mbc1_ramsize_code = 0x00;
static int mbc1_romsize = 0;
static int mbc1_ramsize;

// ROM and RAM
static uint8_t* rom = NULL;
static uint8_t* ram = NULL;

// Registers
static uint8_t ram_enable = 0x00;
static uint8_t rom_bank = 0x00;
static uint8_t rom_bank_2 = 0x00;
static uint8_t mode = 0x00;

uint8_t cart_mbc1_init(uint8_t romsize, uint8_t ramsize, int file_size, uint8_t* temp_rom, uint8_t cart_type)
{
	mbc1_romsize_code = romsize;
	mbc1_ramsize_code = ramsize;
	mbc1_romsize = 32LL * 1024LL * (long long)pow(2, mbc1_romsize_code);
	mbc1_ramsize = 2LL * 1024LL * (long long)pow(4, mbc1_ramsize_code - 1);

	rom = (uint8_t*)calloc(mbc1_romsize, sizeof(uint8_t));
	
	if (rom == NULL)
		return 1;
	int res = memcpy_s(rom, mbc1_romsize, temp_rom, mbc1_romsize);
	if (res != 0)
	{
		free(rom);
		return 1;
	}
	if (mbc1_ramsize_code)
	{
		ram = (uint8_t*)calloc(mbc1_ramsize, sizeof(uint8_t));
		if (ram == NULL)
		{
			free(rom);
			return 1;
		}
		else if (cart_type == CART_MBC1_RAM_BATTERY && file_size > 32LL * 1024LL * pow(2, romsize))
		{
			// Load RAM from save
			int res = memcpy_s(ram, mbc1_ramsize, temp_rom + mbc1_romsize, file_size - mbc1_romsize);
			if (res != 0)
			{
				free(rom);
				free(ram);
				return 1;
			}
		}
	}
	else
	{
		ram = NULL;
	}
	return 0;
}

void cart_mbc1_free()
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

void cart_mbc1_reset()
{
	ram_enable = 0x00;
	rom_bank = 0x00;
	rom_bank_2 = 0x00;
	mode = 0x00;
}

uint8_t cart_mbc1_save(FILE* save_file)
{
	int res = fwrite(rom, sizeof(uint8_t), mbc1_romsize, save_file);
	if (res == 0)
	{
		fclose(save_file);
		return 1;
	}
	res = fwrite(ram, sizeof(uint8_t), mbc1_ramsize, save_file);
	if (res == 0)
	{
		fclose(save_file);
		return 1;
	}

	fclose(save_file);
	return 0;
}

uint8_t cart_mbc1_write(uint16_t addr, uint8_t data)
{
	if (addr >= 0x0000 && addr <= 0x1FFF)
	{
		ram_enable = data;
	}
	else if (addr >= 0x2000 && addr <= 0x3FFF)
	{
		rom_bank = data;
	}
	else if (addr >= 0x4000 && addr <= 0x5FFF)
	{
		rom_bank_2 = data;
	}
	else if (addr >= 0x6000 && addr <= 0x7FFF)
	{
		mode = data;
	}
	else if (addr >= 0xA000 && addr <= 0xBFFF)
	{
		// Depends on the mode, if RAM exists, if RAM is enabled
		if ((ram != NULL) && ((ram_enable & 0x0F) == 0x0A))
		{
			if (((mode & 0x01) == 0x00) || (((mode & 0x01) == 0x01) && (mbc1_ramsize_code < CART_RAM_32K)))
			{
				// If mode is 0 or mode is 1 and RAM size is less than 32KB then RAM banking is disabled
				if (!(mbc1_ramsize_code == CART_RAM_2K && addr >= 0xA800))
					ram[addr - 0xA000] = data;
			}
			else
			{
				// If mode is 1 and RAM size == 32KB RAM banking is enabled
				ram[(addr - 0xA000) + (RAMBANK_SIZE * (rom_bank_2 & 0x03))] = data;
			}
		}
	}
	return 0;
}

uint8_t cart_mbc1_read(uint16_t addr)
{
	if (addr >= 0x0000 && addr <= 0x3FFF)
	{
		if ((mode & 0x01) == 0x01)
		{
			switch (mbc1_romsize_code) {
				case CART_ROM_32K:
				case CART_ROM_64K:
				case CART_ROM_128K:
				case CART_ROM_256K:
				case CART_ROM_512K: // 32 banks: 00 - 1F
					return rom[addr]; // Only bank 00 is available
				case CART_ROM_1M:	// 64 banks: 00 - 3F
					return rom[addr + ROMBANK_SIZE * ((rom_bank_2 & 0x01) << 5)];
				case CART_ROM_2M:	// 128 banks: 00 - 7F
					return rom[addr + ROMBANK_SIZE * ((rom_bank_2 & 0x03) << 5)];
			}
		}
		else
		{
			// In mode 0 only bank 00 is available
			return rom[addr];
		}
	}
	else if (addr >= 0x4000 && addr <= 0x7FFF)
	{
		// Don't care about modes
		uint8_t bank_num = rom_bank & 0x1F; // First 5 bits
		if (bank_num == 0x00)
			bank_num++;
		bank_num &= 0x1F;
		bank_num |= ((rom_bank_2 & 0x03) << 5);

		// Mask the bank number based on the ROM size
		return rom[addr - 0x4000 + ROMBANK_SIZE * (uint8_t)(bank_num & (uint8_t)~(0xFF << (mbc1_romsize_code + 1)))];
	}
	else if (addr >= 0xA000 && addr <= 0xBFFF)
	{
		if (ram != NULL && ram_enable > 0)
		{
			if ((mode & 0x01) == 1 && mbc1_ramsize_code > CART_RAM_8K)
			{
				return ram[addr - 0xA000 + RAMBANK_SIZE * (rom_bank_2 & 0x03)];
			}
			else
			{
				return ram[addr - 0xA000];
			}
		}
	}
	return 0xFF;
}

uint16_t cart_mbc1_compute_global_checksum()
{ 
	uint16_t global_checksum = 0;
	for (int i = 0; i < mbc1_romsize; i++)
	{
		if (i != 0x14E && i != 0x014F)
			global_checksum += rom[i];
	}
	return global_checksum;
}