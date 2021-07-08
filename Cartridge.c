#include "Cartridge.h"

// Cartridge ROM structure
// 0000 - 00FF: Various
// 0100 - 014F: Cartridge header
// 
// 0100 - 0103: Entry point - jump somewhere
// 0104 - 0133: Nintendo logo - this is checked by the boot ROM
// 0134 - 0143: Title - in uppercase ASCII characters
// 013F - 0142: Manufacturer code - in newer cartridges
// 0143		  : CGB flag - in newer cartridges, $C0 - CGB only, $80 - CGB and older
// 0144 - 0145: New licensee code
// 0146		  : SGB flag - $03 - supports SGB, $00 - Doesn't support SGB
// 0147		  : Cartridge type and features
// 0148		  : ROM size
// 0149		  : RAM size
// 014A		  : Destination code - $00 - Japan, $01 - else
// 014B		  : Old licensee code
// 014C		  : ROM version
// 014D		  : Header checksum - this is checked by the boot ROM
// 014E - 014F: Global checksum - isn't checked
//
//
// ROM size value table:
// +------+--------+------------------------+
// | Code |  Size  |		 Banks			|
// +------+--------+------------------------+
// | 0x00 | 32 KB  | No banks				|
// +------+--------+------------------------+
// | 0x01 | 64 KB  | 4 banks				|
// +------+--------+------------------------+
// | 0x02 | 128 KB | 8 banks				|
// +------+--------+------------------------+
// | 0x03 | 256 KB | 16 banks				|
// +------+--------+------------------------+
// | 0x04 | 512 KB | 32 banks				|
// +------+--------+------------------------+
// | 0x05 | 1 MB   | 64 banks (MBC1: 63)	|
// +------+--------+------------------------+
// | 0x06 | 2 MB   | 128 banks (MBC1: 125)	|
// +------+--------+------------------------+
// | 0x07 | 4 MB   | 256 banks				|
// +------+--------+------------------------+
// | 0x08 | 8 MB   | 512 banks				|
// +------+--------+------------------------+
// | 0x52 | 1.1 MB | 72 banks				|
// +------+--------+------------------------+
// | 0x53 | 1.2 MB | 80 banks				|
// +------+--------+------------------------+
// | 0x54 | 1.3 MB | 96 banks				|
// +------+--------+------------------------+
//
//
// RAM size value table:
// +------+--------+----------------+
// | Code |  Size  |    Banks		|
// +------+--------+----------------+
// | 0x00 | None   | None			|
// +------+--------+----------------+
// | 0x01 | 2 KB   | None			|
// +------+--------+----------------+
// | 0x02 | 8 KB   | None			|
// +------+--------+----------------+
// | 0x03 | 32 KB  | 4 banks of 8KB |
// +------+--------+----------------+
// | 0x04 | 128 KB | 16 banks		|
// +------+--------+----------------+
// | 0x05 | 64 KB  | 8 banks		|
// +------+--------+----------------+
// Notes:
// For MBC2, 0x00 must be specified even though it has RAM of 2 MB.

// Initialize static variables
static uint8_t romtype = 0x00;
static uint8_t romsize = 0x00;
static uint8_t ramsize = 0x00;
static uint8_t(*cart_write)(uint16_t addr, uint8_t data) = NULL;
static uint8_t(*cart_read)(uint16_t addr) = NULL;
static void(*cart_free)() = NULL;
static void(*cart_reset)() = NULL;
static uint16_t(*cart_compute_global_checksum)() = NULL;
static uint8_t(*cart_save)(FILE* save_file) = NULL;
bool cart_loaded = false;

uint8_t cart_load(char* filename)
{
	// Check that there isn't a loaded cartridge already in
	if (cart_loaded)
		return CARTERR_LOADED_ALREADY; // Cartridge loaded already

	uint8_t* temp_rom;
	FILE* romfile;
	long file_size;

	// Open ROM file
	errno_t res = fopen_s(&romfile, filename, "rb");
	if (res != 0 || romfile == NULL)
		return CARTERR_FILE_OPEN_ERROR; // Error opening file

	// obtain file size:
	fseek(romfile, 0, SEEK_END);
	file_size = ftell(romfile);
	rewind(romfile);

	// File size must be at least 32KB
	if (file_size < 32 * 1024)
		return CARTERR_FILE_TOO_SMALL; // File is too small

	// allocate memory to contain the whole file:
	temp_rom = (uint8_t*)malloc(sizeof (uint8_t) * file_size);
	if (temp_rom == NULL)
		return CARTERR_ALLOC_ERROR; // Error allocating memory

	// copy the file into the buffer:
	size_t result = fread(temp_rom, 1, file_size, romfile);
	if (result != file_size)
		return CARTERR_FILE_READ_ERROR; // Error reading file

	// Close file
	fclose(romfile);

	// The whole file is now loaded in temp_rom
	// Check for correct 
	romtype = temp_rom[0x0147];
	romsize = temp_rom[0x0148];
	ramsize = temp_rom[0x0149];
	uint8_t type_error = 0x00;

	if (romtype == CART_ROM_ONLY)
	{
		// Should be 32 KB ROM and up to 8K RAM
		if (romsize != CART_ROM_32K)
			type_error = CARTERR_ROM_SIZE_ERROR; // ROM size header error
		else if (ramsize > CART_RAM_8K)
			type_error = CARTERR_RAM_SIZE_ERROR; // RAM size header error
		else if (file_size != 32 * 1024)
			type_error = CARTERR_FILE_SIZE_ERROR; // File size error
		else
		{
			cart_rom_only_init(romsize, ramsize, temp_rom);
			cart_write = &cart_rom_only_write;
			cart_read = &cart_rom_only_read;
			cart_free = &cart_rom_only_free;
			cart_reset = &cart_rom_only_reset;
			cart_compute_global_checksum = &cart_rom_only_compute_global_checksum;
			cart_save = NULL;
			cart_loaded = true;
		}
	}
	else if (romtype >= CART_MBC1 && romtype <= CART_MBC1_RAM_BATTERY)
	{
		// Should have up to 2MB of ROM and up to 32KB of RAM
		// Types:
		// CART_MBC1
		// CART_MBC1_RAM
		// CART_MBC1_RAM_BATTERY
		if (romsize > CART_ROM_2M)
			type_error = CARTERR_ROM_SIZE_ERROR; // ROM size header error
		else if (ramsize > CART_RAM_32K) // Not specifying having RAM is ok by me 
			type_error = CARTERR_RAM_SIZE_ERROR; // RAM size header error
		else if (file_size < 32LL * 1024LL * pow(2, romsize))
			type_error = CARTERR_FILE_SIZE_ERROR; // File size error
		else
		{
			uint8_t res = cart_mbc1_init(romsize, ramsize, file_size, temp_rom, romtype);
			if (res != 0)
				type_error = CARTERR_ALLOC_ERROR;
			cart_write = cart_mbc1_write;
			cart_read = cart_mbc1_read;
			cart_free = cart_mbc1_free;
			cart_reset = cart_mbc1_reset;
			cart_compute_global_checksum = cart_mbc1_compute_global_checksum;
			cart_save = cart_mbc1_save;
			cart_loaded = true;
		}
	}
	else
	{
		type_error = CARTERR_CART_NOT_SUPPORTED; // Not supported
	}
	

	// terminate
	free(temp_rom);
	return type_error;
}

uint8_t cart_unload()
{
	if (cart_loaded)
	{
		(*cart_free)();
		cart_loaded = false;
	}
	return 0;
}

uint8_t cart_mapper_write(uint16_t addr, uint8_t data)
{
	if (cart_loaded)
		(*cart_write)(addr, data);
	return 0;
}

uint8_t cart_mapper_read(uint16_t addr)
{
	if (cart_loaded)
		return (*cart_read)(addr);
	else
		return 0xFF;
}

void cart_mapper_reset()
{
	if (cart_loaded)
		(*cart_reset)();
}

uint16_t cart_mapper_compute_global_checksum() 
{
	if (cart_loaded)
		return (*cart_compute_global_checksum)();
	else
		return 0xFFFF;
}

uint8_t cart_mapper_save(char* filename)
{
	if (!cart_loaded)
		return 1;
	switch (romtype)
	{
		case CART_MBC1_RAM_BATTERY:
		case CART_MBC2_BATTERY:
		case CART_ROM_RAM_BATTERY:
		case CART_MMM01_RAM_BATTERY:
		case CART_MBC3_TIMER_RAM_BATTERY:
		case CART_MBC3_RAM_BATTERY:
		case CART_MBC5_RAM_BATTERY:
		case CART_MBC5_RUMBLE_RAM_BATTERY:
		case CART_MBC7_SENSOR_RUMBLE_RAM_BATTERY:
		case CART_HUC1_RAM_BATTERY:
			// You can save
			break;
		default:
			return 1; // Cart doesn't have a RAM with battery
	}
	if (cart_save == NULL)
		return 1;
	FILE* save_file;
	errno_t res = fopen_s(&save_file, filename, "wb");
	if (res != 0)
		return 1;
	uint8_t ress = (*cart_save)(save_file);
	return ress;
}

uint8_t cart_get_stats(int nTitle, char* title, uint8_t* type, uint8_t* rom_size, uint8_t* ram_size, uint8_t* japan, 
	bool* header_chck, bool* global_check)
{
	// Cartridge header memory map
	// $0100 - $0103: Entry point
	// $0104 - $0133: Nintedno logo
	// $0134 - $0143: Title and (sometimes) manufacturer code and CGB flag
	// $0144 - $0145: New licensee code
	// $0146: SGB flag
	// $0147: Cartridge type
	// $0148: ROM size
	// $0149: RAM size
	// $014A: Destination code (Japan)
	// $014B: Old licensee code
	// $014C: Mask ROM version number
	// $014D: Header checksum (from $0134 to $014C)
	// $014E - $014F: Global checksum
	if (nTitle > 0 && title != NULL)
	{
		char tmp_title[17] = { 0 };
		for (int i = 0; i < 16; i++)
		{
			tmp_title[i] = cart_mapper_read(0x0134 + i);
		}
		strncpy_s(title, nTitle, tmp_title, _TRUNCATE);
	}
	if (type != NULL)
		*type = cart_mapper_read(0x147);
	if (rom_size != NULL)
		*rom_size = cart_mapper_read(0x148);
	if (ram_size != NULL)
		*ram_size = cart_mapper_read(0x149);
	if (japan != NULL)
		*japan = cart_mapper_read(0x14A);
	if (header_chck != NULL)
	{
		uint16_t checksum = 0;
		for (int i = 0; i < 25; i++)
		{
			checksum = checksum - cart_mapper_read(0x0134 + i) - 1;
		}
		*header_chck = ((uint8_t)checksum == cart_mapper_read(0x14D));
	}
	if (global_check != NULL)
		*global_check = (cart_mapper_compute_global_checksum() == (uint16_t)((uint16_t)(cart_mapper_read(0x14E) << 8) + cart_mapper_read(0x14F)));
	return 0;
}

