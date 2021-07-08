#ifndef CART_CODE
#define CART_CODE

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include "Bus.h"
#include "Cart_rom_only.h"
#include "Cart_mbc1.h"

// Load ROM file based on type, ROM and RAM size
uint8_t cart_load(char* filename);
uint8_t cart_unload();

// Mapper read and write
uint8_t cart_mapper_write(uint16_t addr, uint8_t data);
uint8_t cart_mapper_read(uint16_t addr);

// Reset
void cart_mapper_reset();

uint16_t cart_mapper_compute_global_checksum();

uint8_t cart_mapper_save(char* filename);

uint8_t cart_get_stats(int nTitle, char* title, uint8_t* type, uint8_t* rom_size, uint8_t* ram_size,
	uint8_t* japan, bool* header_chck, bool* global_check);
#endif // CART_CODE