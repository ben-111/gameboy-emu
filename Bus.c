#include "Bus.h"
#include "Sharp_LR35902.h"
#include "Ppu.h"
#include "Cartridge.h"
#include "Emulator_GUI.h"
#include "Dma.h"
#include "Timer.h"
#include "Joypad.h"


// GameBoy variants:
// DMG - Original GameBoy
// SGB - Super GameBoy
// CGB - Color GameBoy
// Pocket GameBoy
// GBA - GameBoy Advanced
// 
// Memory Map:
//
// $0000 - $00FF: Boot ROM
// $0000 - $0038: RST vectors
// $0040 - $0058: Interrupt vectors
// $0000 - $3FFF: From cartridge, usually a fixed bank
// $4000 - $7FFF: From cartridge, switchable bank (if any)
// $8000 - $9FFF: VRAM (Video RAM)
//		$8000 - $87FF: Pattern bank A
//		$8800 - $8FFF: Pattern bank B
//		$9000 - $97FF: Pattern bank C
//		$9800 - $9BFF: Tile map A
//		$9C00 - $9FFF: Tile map B
// $A000 - $BFFF: External RAM in cartridge, switchable bank if any
// $C000 - $DFFF: WRAM (Work RAM)
// $E000 - $FDFF: Echo RAM of $CF000 - $DDFF, shouldn't really be used
// $FE00 - $FE9F: OAM - Object Attribute Memory, basically sprite attribute table
// $FEA0 - $FEFF: Not usable
// $FF00 - $FF7F: I/O registers:
//		$FF00: Port P1 - Joypad
//		$FF01: SB - Serial data register
//		$FF02: SC - Serial control register
//		$FF03: ---
//		$FF04: DIV - Divider register
//		$FF05: TIMA - Timer counter register
//		$FF06: TMA - Timer Modulo register
//		$FF07: TAC - Timer Control register
//		$FF08 - $FF0E: ---
//		$FF0F: IF - Interrupts Flag register
//		$FF10 - $FF14: NR10 - NR14, Sound channel 1 registers
//		$FF15: ---
//		$FF16 - $FF19: NR21 - NR24, Sound channel 2 registers
//		$FF1A - $FF1E: NR30 - NR34, Sound channel 2 registers
//		$FF1F: ---
//		$FF20 - $FF23: NR41 - NR44, Sound channel 4 registers
//		$FF24 - $FF26: NR50 - NR52, Sound channel 5 registers
//		$FF27 - $FF2F: --
//		$FF30 - $FF3F: WAV00 - WAV15, Sound wave samples
//		$FF40: LCDC - LCD control register
//		$FF41: STAT - LCD status register
//		$FF42: SCY - Vertical scroll register
//		$FF43: SCX - Horizontal scroll register
//		$FF44: LY - Scanline register
//		$FF45: LYC - Scanline compare register
//		$FF46: DMA - OAM DMA control register
//		$FF47: BGP - Background Palette data register
//		$FF48: OBP0 - Object Palette 0 data register
//		$FF49: OBP1 - Object Palette 1 data register
//		$FF4A: WY - Window Y position
//		$FF4B: WX - Window X position
//		$FF4C: ????
//		$FF4D: KEY1 - CGB only
//		$FF4E: ---
//		$FF4F: VBK - CGB only
//		$FF50: BOOT - Disables mapping of the boot ROM
//		$FF51 - $FF55: HDMA1 - HDMA5, CGB only
//		$FF56: RP - CGB only
//		$FF57 - $FF67: ---
//		$FF68: BCPS - CGB only
//		$FF69: BCPD - CGB only
//		$FF6A: OCPS - CGB only
//		$FF6B: OCPD - CGB only
//		$FF6C: OPRI - CGB only
//		$FF6D - $FF6F: ---
//		$FF70: SVBK - CGB only
//		$FF71: ---
//		$FF72 - $FF75: Undocumented
//		$FF76: PCM12 - Undocumented read only PCM amplitudes of channels 1 & 2 
//		$FF77: PCM34 - Undocumented read only PCM amplitudes of channels 3 & 4
//		$FF78 - $FF7F: ---
// $FF80 - $FFFE: HRAM (High RAM)
// $FFFF: IE - Interrupts Enable register

// Static variables
// Boot ROM: 0000 - 00FF
static uint8_t bootrom[256] = {
0x31, 0xFE, 0xFF,	// 0000: LD SP, $FFFE
0xAF,				// 0003: XOR A
0x21, 0xFF, 0x9F,	// 0004: LD HL, $9FFF
0x32,				// 0007: LD (HL-), A
0xCB, 0x7c,			// 0008: BIT 7, H
0x20, 0xFB,			// 000A: JR NZ, $FB [-5 => 0007]
0x21, 0x26, 0xFF,	// 000C: LD HL, $FF26
0x0E, 0x11,			// 000F: LD C, $11
0x3E, 0x80,			// 0011: LD A, $80
0x32,				// 0013: LD (HL-), A
0xE2,				// 0014: LD ($FF00+C), A
0x0C,				// 0015: INC C
0x3E, 0xF3,			// 0016: LD A, $F3
0xE2,				// 0018: LD ($FF00+C), A
0x32,				// 0019: LD (HL-), A
0x3E, 0x77,			// 001A: LD A, $77
0x77,				// 001C: LD (HL), A
0x3E, 0xFC,			// 001D: LD A, $FC
0xE0, 0x47,			// 001F: LD A, ($FF00+$47)
0x11, 0x04, 0x01,	// 0021: LD DE, $0104
0x21, 0x10, 0x80,	// 0024: LD HL, $8010
0x1A,				// 0027: LD A, (DE)
0xCD, 0x95, 0x00,	// 0028: CALL $0095
0xCD, 0x96, 0x00,	// 002B: CALL $0096
0x13,				// 002E: INC DE
0x7B,				// 002F: LD A, E
0xFE, 0x34,			// 0030: CP $34
0x20, 0xF3,			// 0032: JR NZ, $F3 [-13 => 0027]
0x11, 0xD8, 0x00,	// 0034: LD DE, $00D8
0x06, 0x08,			// 0037: LD B, $08
0x1A,				// 0039: LD A, (DE)
0x13,				// 003A: INC DE
0x22,				// 003B: LD (HL+), A
0x23,				// 003C: INC HL
0x05,				// 003D: DEC B
0x20, 0xF9,			// 003E: JR NZ, $F9 [-7 => 0039]
0x3E, 0x19,			// 0040: LD A, $19
0xEA, 0x10, 0x99,	// 0042: LD ($9910), A
0x21, 0x2F, 0x99,	// 0045: LD HL, $992F
0x0E, 0x0C,			// 0048: LD C, $0C
0x3D,				// 004A: DEC A
0x28, 0x08,			// 004B: JR Z, $08 [+8 => 0055]
0x32,				// 004D: LD (HL-), A
0x0D,				// 004E: DEC C
0x20, 0xF9,			// 004F: JR NZ, $F9 [-7 => 004A]
0x2E, 0x0F,			// 0051: LD L, $0F
0x18, 0xF3,			// 0053: JR $F3 [-13 => 0048]
0x67,				// 0055: LD H, A
0x3E, 0x64,			// 0056: LD A, $64
0x57,				// 0058: LD D, A
0xE0, 0x42,			// 0059: LD ($FF00+$42), A	; Set vertical scroll register
0x3E, 0x91,			// 005B: LD A, $91
0xE0, 0x40,			// 005D: LD ($FF00+$40), A	; Turn on LCD, show background
0x04,				// 005F: INC B
0x1E, 0x02,			// 0060: LD E, $02
0x0E, 0x0C,			// 0062: LD C, $0C
0xF0, 0x44,			// 0064: LD A, ($FF00+$44)	; Read line register
0xFE, 0x90,			// 0066: CP $90				; Wait for the frame to draw
0x20, 0xFA,			// 0068: JR NZ, $FA [-6 => 0064]
0x0D,				// 006A: DEC C				; Do it 12 times
0x20, 0xF7,			// 006B: JR NZ, $F7 [-9 => 0064]
0x1D,				// 006D: DEC E
0x20, 0xF2,			// 006E: JR NZ, $F2 [-14 => 0062]
0x0E, 0x13,			// 0070: LD C, $13
0x24,				// 0072: INC H
0x7C,				// 0073: LD A, H
0x1E, 0x83,			// 0074: LD E, $83
0xFE, 0x62,			// 0076: CP $62
0x28, 0x06,			// 0078: JR Z, $06 [+6 => 0080]
0x1E, 0xC1,			// 007A: LD E, $C1
0xFE, 0x64,			// 007C: CP $64
0x20, 0x06,			// 007E: JR NZ, $06 [+6 => 0086]
0x7B,				// 0080: LD A, E
0xE2,				// 0081: LD ($FF00+C), A
0x0C,				// 0082: INC C
0x3E, 0x87,			// 0083: LD A, $87
0xE2,				// 0085: LD ($FF00+C), A
0xF0, 0x42,			// 0086: LD A, ($FF00+$42)
0x90,				// 0088: SUB B
0xE0, 0x42,			// 0089: LD ($FF00+$42), A
0x15,				// 008B: DEC D
0x20, 0xD2,			// 008C: JR NZ, $D2 [-46 => 0060]
0x05,				// 008E: DEC B
0x20, 0x4F,			// 008F: JR NZ, $4F [+79 => 00E0]
0x16, 0x20,			// 0091: LD D, $20
0x18, 0xCB,			// 0093: JR $CB [-53 => 0060]
0x4F,				// 0095: LD C, A
0x06, 0x04,			// 0096: LD B, $04
0xC5,				// 0098: PUSH BC
0xCB, 0x11,			// 0099: RL C
0x17,				// 009B: RLA
0xC1,				// 009C: POP BC
0xCB, 0x11,			// 009D: RL C
0x17,				// 009F: RLA
0x05,				// 00A0: DEC B
0x20, 0xF5,			// 00A1: JR NZ, $F5 [-11 => 0098]
0x22,				// 00A3: LD (HL+), A
0x23,				// 00A4: INC HL
0x22,				// 00A5: LD (HL+), A
0x23,				// 00A6: INC HL
0xC9,				// 00A7: RET
// 00A8: Nintendo Logo
0xCE, 0xED,	0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 
0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 
0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 
// 00D8: Other video data
0x3C, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x3C, 
0x21, 0x04, 0x01,	// 00E0: LD HL, $0104
0x11, 0xA8, 0x00,	// 00E3: LD DE, $00A8
0x1A,				// 00E6: LD A, (DE)
0x13,				// 00E7: INC DE
0xBE,				// 00E8: CP (HL)
0x20, 0xFE,			// 00E9: JR NZ, $FE [-2 => 00E9]
0x23,				// 00EB: INC HL
0x7D,				// 00EC: LD A, L
0xFE, 0x34,			// 00ED: CP $34
0x20, 0xF5,			// 00EF: JR NZ, $F5 [-11 => 00E6]
0x06, 0x19,			// 00F1: LD B, $19
0x78,				// 00F3: LD A, B
0x86,				// 00F4: ADD A, (HL)
0x23,				// 00F5: INC HL
0x05,				// 00F6: DEC B
0x20, 0xFB,			// 00F7: JR NZ, $FB [-5 => 00F4]
0x86,				// 00F9: ADD A, (HL)
0x20, 0xFE,			// 00FA: JR NZ, $FE [-2 => 00FA]
0x3E, 0x01,			// 00FC: LD A, $01
0xE0, 0x50			// 00FE: LD ($FF00+$50), A
};

// Work RAM: C000 - DFFF
static uint8_t wram[8192];

// IF - Interrupt flag register: FF0F
static uint8_t IF = 0x00;

// Boot ROM disabled: FF50
static uint8_t BOOTROM_REG = 0x00;

// High RAM: FF80 - FFFE
static uint8_t hram[127];

// IE - Interrupt enable register: FFFF
static uint8_t IE = 0x00;

// DMA occuring
static bool dma_transfer = false;
static uint8_t dma_count = 0x00;

// Screen buffer
static uint32_t rgb_screen_buffer[160 * 144];

// Define globals
bool cpu_halt = false;
bool cpu_int_check = false;
uint64_t clock_count = 0;


int gameboy_reset(bool bootskip)
{
	// Reset memory
	memset(wram, 0, sizeof wram);
	memset(hram, 0, sizeof hram);
	memset(rgb_screen_buffer, 0, sizeof rgb_screen_buffer);
	
	// Turn screen off
	gameboy_screen_off();

	cpu_halt = false;
	cpu_int_check = false;
	clock_count = 0;

	// Reset registers
	IF = 0x00;
	
	BOOTROM_REG = 0x00;
	IE = 0x00;
	
	if (bootskip)
	{
		BOOTROM_REG = 0x01;
	}
	cpu_reset(bootskip);
	dma_transfer = false;
	dma_count = 0x00;
	dma_reset();
	if (ppu_reset(bootskip) != 0)
	{
		return 1;
	}
	timer_reset();
	joypad_reset();
	cart_mapper_reset();
	return 0;
}

void gameboy_clock()
{
	
	if (!cpu_halt && (clock_count) % 4 == 0)
	{
		cpu_clock();
		if (dma_transfer)
		{
			dma_clock();
			dma_count++;
			if (dma_count == 160)
				dma_transfer = false;
		}
	}
	timer_clock();
	ppu_clock();
	clock_count++;
}

void gameboy_mclock()
{
	for (int i = 0; i < 4; i++)
	{
		gameboy_clock();
	}
}

uint8_t gameboy_cart_load(char* filename)
{
	return cart_load(filename);
}

uint8_t gameboy_cart_unload()
{
	return cart_unload();
}

uint8_t bus_write(uint16_t addr, uint8_t data, uint8_t device)
{
	if (!dma_transfer || device == DEV_DMA)
	{
		if (addr >= 0x0000 && addr <= 0x7FFF)
		{
			if (!BOOTROM_REG && addr <= 0x0100)
			{
				// Do nothing
			}
			else
			{
				cart_mapper_write(addr, data);
			}
		}
		else if (addr >= 0x8000 && addr <= 0x9FFF)
		{
			vram_write(addr, data);
		}
		else if (addr >= 0xA000 && addr <= 0xBFFF)
		{
			cart_mapper_write(addr, data);
		}
		else if (addr >= 0xC000 && addr <= 0xDFFF)
			wram[addr - 0xC000] = data;
		else if (addr >= 0xE000 && addr <= 0xFDFF)
			wram[addr - 0xE000] = data;
		else if (addr >= 0xFE00 && addr <= 0xFE9F)
		{
			oam_write(addr, data, device);
		}
		// IO registers
		else if (addr >= 0xFF00 && addr <= 0xFF7F)
		{
			switch (addr)
			{
			case 0xFF00:
				joypad_register_write(addr, data);
				break;
			case 0xFF04:
			case 0xFF05:
			case 0xFF06:
			case 0xFF07:
				timer_register_write(addr, data);
				break;
			case 0xFF0F:
				if (cpu_halt)
					cpu_halt = false;
				cpu_int_check = true;
				IF = data;
				break;
			case 0xFF40:
			case 0xFF41:
			case 0xFF42:
			case 0xFF43:
			case 0xFF45:
			case 0xFF46:
				if (addr == 0xFF46)
				{
					dma_transfer = true;
					dma_count = 0;
				}
			case 0xFF47:
			case 0xFF48:
			case 0xFF49:
			case 0xFF4A:
			case 0xFF4B:
				ppu_register_write(addr, data);
				break;
			case 0xFF50:
				if (!BOOTROM_REG)
					BOOTROM_REG = data;
				break;
			default:
				break;
			}
			
		}
		else if (addr == 0xFFFF)
		{
		if (cpu_halt)
			cpu_halt = false;
		if (data != IE && data != 0)
			cpu_int_check = true;
		IE = data;
		}
	}
	if (addr >= 0xFF80 && addr <= 0xFFFE)
	{
		hram[addr - 0xFF80] = data;
	}

	
	return 0;
}

uint8_t bus_read(uint16_t addr, uint8_t device)
{
	if (addr >= 0x0000 && addr <= 0x7FFF)
	{
		if (!BOOTROM_REG && addr < 0x0100)
		{
			return bootrom[addr];
		}
		return cart_mapper_read(addr);
	}
	else if (addr >= 0x8000 && addr <= 0x9FFF)
	{
		return vram_read(addr);
	}
	else if (addr >= 0xA000 && addr <= 0xBFFF)
	{
		return cart_mapper_read(addr);
	}
	else if (addr >= 0xC000 && addr <= 0xDFFF)
		return wram[addr - 0xC000];
	else if (addr >= 0xE000 && addr <= 0xFDFF)
		return wram[addr - 0xE000];
	else if (addr >= 0xFE00 && addr <= 0xFE9F)
	{
		return oam_read(addr);
	}
	// IO registers
	else if (addr >= 0xFF00 && addr <= 0xFF7F)
	{
		switch (addr) 
		{
			case 0xFF00:
				return joypad_register_read(addr);
			case 0xFF04:
			case 0xFF05:
			case 0xFF06:
			case 0xFF07:
				return timer_register_read(addr);
			case 0xFF0F:
				return IF;
			case 0xFF40:
			case 0xFF41:
			case 0xFF42:
			case 0xFF43:
			case 0xFF44:
			case 0xFF45:
			case 0xFF46:
			case 0xFF47:
			case 0xFF48:
			case 0xFF49:
			case 0xFF4A:
			case 0xFF4B:
				return ppu_register_read(addr);
			case 0xFF50:
				return BOOTROM_REG;
			default:
				return 0xFF;
		}
	}
	else if (addr >= 0xFF80 && addr <= 0xFFFE)
	{
		return hram[addr - 0xFF80];
	}
	else if (addr == 0xFFFF)
		return IE;
	else
		return 0xFF;
}

void gameboy_cpu_stats(uint16_t* af, uint16_t* bc, uint16_t* de, uint16_t* hl, uint16_t* sp, uint16_t* pc, bool* ime, uint8_t* stat_opcode, 
	uint8_t* stat_cycles, uint8_t* stat_fetched, uint16_t* stat_fetched16, uint8_t* cb_op, uint16_t* stat_stackbase)
{
	cpu_get_stats(
		af, 
		bc, 
		de, 
		hl, 
		sp, 
		pc, 
		ime, 
		stat_opcode, 
		stat_cycles, 
		stat_fetched, 
		stat_fetched16,
		cb_op,
		stat_stackbase);
}

uint8_t gameboy_cpu_disassemble_inst(uint16_t inst_pointer, wchar_t* disassembled_inst, int strlen)
{
	return disassemble_inst(inst_pointer, disassembled_inst, strlen);
}

void gameboy_screen_set_pixel(int x, int y, COLOR2BIT color2bit)
{
	if (x >= 160 || y >= 144 || x < 0 || y < 0)
	{
		return;
	}
	// 00 - white #f0f0f0, 01 - light gray #a0a0a0, 10 - dark gray #505050, 11 - black #000000
	rgb_screen_buffer[160 * y + x] = RGB(0x50 * (~color2bit & 0x03), 0x50 * (~color2bit & 0x03), 0x50 * (~color2bit & 0x03));
}

void gameboy_screen_on()
{
	COLOR2BIT clrOn = 0b00;

	for (int i = 0; i < 160; i++)
	{
		for (int j = 0; j < 144; j++)
			gameboy_screen_set_pixel(i, j, clrOn);
	}

}

void gameboy_screen_off()
{
	for (int i = 0; i < 160 * 144; i++)
	{
		rgb_screen_buffer[i] = RGB(255, 255, 255);
	}
}

uint32_t* gameboy_get_screen()
{
	return rgb_screen_buffer;
}

void gameboy_ppu_stats(uint8_t* lcdc, uint8_t* stat, uint8_t* scy, uint8_t* scx, uint8_t* ly, uint8_t* lyc, uint8_t* bgp, uint8_t* obp0, uint8_t* obp1, uint8_t* wy, uint8_t* wx)
{
	ppu_get_stats(lcdc,
		stat,
		scy,
		scx,
		ly,
		lyc,
		bgp,
		obp0,
		obp1,
		wy,
		wx);
}

uint8_t gameboy_joypad_input(uint8_t button, bool bPressed)
{
	return joypad_button_press(button, bPressed);
}

uint8_t gameboy_cartridge_stats(int nTitle, char* title, uint8_t* type, uint8_t* rom_size, uint8_t* ram_size, 
	uint8_t* japan, bool* header_chck, bool* global_check)
{
	return cart_get_stats(nTitle, title, type, rom_size, ram_size, japan, header_chck, global_check);
}

uint8_t gameboy_cartridge_save(char* filename)
{
	return cart_mapper_save(filename);
}
