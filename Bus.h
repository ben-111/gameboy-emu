#ifndef BUS_CODE
#define BUS_CODE

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Gameboy reset
int gameboy_reset(bool bootskip);

// Gameboy cycle (T-State and M-State)
void gameboy_clock();
void gameboy_mclock();

// Gameboy load and unload cartridge
uint8_t gameboy_cart_load(char* filename);
uint8_t gameboy_cart_unload();

// Global clock counter
extern uint64_t clock_count;

// Bus write
uint8_t bus_write(uint16_t addr, uint8_t data, uint8_t device);

// Bus read
uint8_t bus_read(uint16_t addr, uint8_t device);

// Devices
enum DEVICES {
	DEV_CPU,
	DEV_PPU,
	DEV_DMA,
	DEV_APU,
	DEV_TIMER,
	DEV_JOYPAD
};

// CPU halt flag
extern bool cpu_halt;

// Interrupt flags
enum INT_FLAGS {
	INT_VBLANK = (1 << 0),
	INT_LCDSTAT = (1 << 1),
	INT_TIMER = (1 << 2),
	INT_SERIAL = (1 << 3),
	INT_JOYPAD = (1 << 4)
};

// IE and IF registers' addresses
#define IE_ADDR 0xFFFF
#define IF_ADDR 0xFF0F

// CPU stats
void gameboy_cpu_stats(uint16_t* af, uint16_t* bc, uint16_t* de, uint16_t* hl, uint16_t* sp, uint16_t* pc,
	bool* ime, uint8_t* stat_opcode, uint8_t* stat_cycles, uint8_t* stat_fetched, uint16_t* stat_fetched16, 
	uint8_t* cb_op, uint16_t* stat_stackbase);

uint8_t gameboy_cpu_disassemble_inst(uint16_t inst_pointer, wchar_t* disassembled_inst, int strlen);

// Screen related
typedef uint8_t COLOR2BIT;
void gameboy_screen_set_pixel(int x, int y, COLOR2BIT color2bit);
void gameboy_screen_on();
void gameboy_screen_off();
uint32_t* gameboy_get_screen();

// PPU stats
void gameboy_ppu_stats(uint8_t* lcdc, uint8_t* stat, uint8_t* scy, uint8_t* scx, uint8_t* ly, uint8_t* lyc,
	uint8_t* bgp, uint8_t* obp0, uint8_t* obp1, uint8_t* wy, uint8_t* wx);

enum GAMEBOY_JOYPAD_BUTTONS {
	GB_JOYPAD_UP,
	GB_JOYPAD_DOWN,
	GB_JOYPAD_LEFT,
	GB_JOYPAD_RIGHT,
	GB_JOYPAD_START,
	GB_JOYPAD_SELECT,
	GB_JOYPAD_B,
	GB_JOYPAD_A
};

uint8_t gameboy_joypad_input(uint8_t button, bool bPressed);

// Cartrdge errors
enum GAMEBOY_CARTRIDGE_ERRORS {
	CARTERR_LOADED_ALREADY = 0,
	CARTERR_FILE_OPEN_ERROR,
	CARTERR_FILE_TOO_SMALL,
	CARTERR_ALLOC_ERROR,
	CARTERR_FILE_READ_ERROR,
	CARTERR_ROM_SIZE_ERROR,
	CARTERR_RAM_SIZE_ERROR,
	CARTERR_FILE_SIZE_ERROR,
	CARTERR_CART_NOT_SUPPORTED
};

uint8_t gameboy_cartridge_stats(int nTitle, char* title, uint8_t* type, uint8_t* rom_size, uint8_t* ram_size, 
	uint8_t* japan, bool* header_chck, bool* global_check);

enum GAMEBOY_CART_TYPES {
	CART_ROM_ONLY = 0x00,
	CART_MBC1,
	CART_MBC1_RAM,
	CART_MBC1_RAM_BATTERY,
	CART_MBC2 = 0x05,
	CART_MBC2_BATTERY,
	CART_ROM_RAM = 0x08,
	CART_ROM_RAM_BATTERY,
	CART_MMM01 = 0x0B,
	CART_MMM01_RAM,
	CART_MMM01_RAM_BATTERY,
	CART_MBC3_TIMER_BATTERY = 0x0F,
	CART_MBC3_TIMER_RAM_BATTERY,
	CART_MBC3,
	CART_MBC3_RAM,
	CART_MBC3_RAM_BATTERY,
	CART_MBC5 = 0x19,
	CART_MBC5_RAM,
	CART_MBC5_RAM_BATTERY,
	CART_MBC5_RUMBLE,
	CART_MBC5_RUMBLE_RAM,
	CART_MBC5_RUMBLE_RAM_BATTERY,
	CART_MBC6 = 0x20,
	CART_MBC7_SENSOR_RUMBLE_RAM_BATTERY = 0x22,
	CART_POCKET_CAMERA = 0xFC,
	CART_BANDAI_TAMA5,
	CART_HUC3,
	CART_HUC1_RAM_BATTERY
};

enum GAMEBOT_CART_ROM_SIZE {
	CART_ROM_32K = 0,
	CART_ROM_64K,
	CART_ROM_128K,
	CART_ROM_256K,
	CART_ROM_512K,
	CART_ROM_1M,
	CART_ROM_2M,
	CART_ROM_4M,
	CART_ROM_8M,
	CART_ROM_1_1M = 0x52,
	CART_ROM_1_2M,
	CART_ROM_1_5M
};

enum GAMEBOY_CART_RAM_SIZE {
	CART_RAM_NONE = 0,
	CART_RAM_2K,
	CART_RAM_8K,
	CART_RAM_32K,
	CART_RAM_128K,
	CART_RAM_64K
};

uint8_t gameboy_cartridge_save(char* filename);
#endif // BUS_CODE