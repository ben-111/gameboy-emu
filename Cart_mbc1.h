#ifndef MBC1
#define MBC1
#include "Cartridge.h"

uint8_t cart_mbc1_init(uint8_t romsize, uint8_t ramsize, int file_size, uint8_t* temp_rom, uint8_t cart_type);
void cart_mbc1_free();
void cart_mbc1_reset();
uint8_t cart_mbc1_save(FILE* save_file);

uint8_t cart_mbc1_write(uint16_t addr, uint8_t data);
uint8_t cart_mbc1_read(uint16_t addr);

uint16_t cart_mbc1_compute_global_checksum();

#endif // MBC1