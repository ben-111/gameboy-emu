#ifndef ROM_ONLY
#define ROM_ONLY
#include "Cartridge.h"

uint8_t cart_rom_only_init(uint8_t romsize, uint8_t ramsize, uint8_t* temp_rom);
void cart_rom_only_free();
void cart_rom_only_reset();

uint8_t cart_rom_only_write(uint16_t addr, uint8_t data);
uint8_t cart_rom_only_read(uint16_t addr);

uint16_t cart_rom_only_compute_global_checksum();

#endif // ROM_ONLY