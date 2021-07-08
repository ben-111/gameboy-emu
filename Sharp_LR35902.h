#ifndef CPU_CODE
#define CPU_CODE

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <strsafe.h>

// Flags
typedef enum
{
	Cy = (1 << 4),	// Carry
	Hc = (1 << 5),	// Half carry
	N = (1 << 6),	// Add/Sub (0/1)
	Z = (1 << 7),	// Zero
} FLAGSLR35902;

extern bool cpu_int_check;			// Do you need to check for an interrupt?

// Interrupt enable states
enum INT_STATES {
	INT_NO_CHANGE		= 0,
	INT_ENABLE			= 1,
	INT_DELAY_ENABLE	= 2,
	INT_DISABLE			= 3
};



// Interrupt routine addresses
enum INT_ROUTINES {
	INT_VBLANK_RTN	= 0x0040,
	INT_LEDSTAT_RTN = 0x0048,
	INT_TIMER_RTN	= 0x0050,
	INT_SERIAL_RTN	= 0x0058,
	INT_JOYPAD_RTN	= 0x0060
};

uint8_t cpu_getflag(FLAGSLR35902 f);
void cpu_setflag(FLAGSLR35902 f, bool b);
void cpu_set_a(uint8_t data);
void cpu_set_b(uint8_t data);
void cpu_set_c(uint8_t data);
void cpu_set_d(uint8_t data);
void cpu_set_e(uint8_t data);
void cpu_set_h(uint8_t data);
void cpu_set_l(uint8_t data);
void cpu_set_f(uint8_t data);

void cpu_clock();
void cpu_reset(bool bootskip);

uint8_t cpu_read(uint16_t addr);
void cpu_write(uint16_t addr, uint8_t data);

// Get stats for debug
void cpu_get_stats(uint16_t* af, uint16_t* bc, uint16_t* de, uint16_t* hl, uint16_t* sp, uint16_t* pc,
	bool* ime, uint8_t* stat_opcode, uint8_t* stat_cycles, uint8_t* stat_fetched, uint16_t* stat_fetched16, 
	uint8_t* cb_op, uint16_t* stat_stackbase);

// Define instructions

// 8-bit Address modes
uint8_t IMP();		// Implied - nothing
uint8_t IMM();		// Immediate - a number
uint8_t PTR();		// (nn)
uint8_t PTRIO();	// ($FF00+n)
uint8_t RGA();		// Register A
uint8_t RGB();
uint8_t RGC();
uint8_t RGD();
uint8_t RGE();
uint8_t RGH();
uint8_t RGL();
uint8_t RGBC();		// Pointer in BC
uint8_t RGDE();
uint8_t RGHL();
uint8_t RGCIO();	// ($FFO0+C)

// 16-bit Address modes
uint8_t IMM_16();	// Immediate 16 bits
uint8_t RGBC_16();	// Register BC
uint8_t RGDE_16();
uint8_t RGHL_16();
uint8_t RGSP_16();
uint8_t SPSN_16();	// SP + 8-bit signed number


// 8-bit Instructions
uint8_t ADC();		// Add with carry
uint8_t ADD();		// Add
uint8_t AND();		// And
uint8_t BIT0();		// Test bit 0
uint8_t BIT1();
uint8_t BIT2();
uint8_t BIT3();
uint8_t BIT4();
uint8_t BIT5();
uint8_t BIT6();
uint8_t BIT7();
uint8_t CALL();		// Call nn
uint8_t CALLC();	// Call if carry
uint8_t CALLNC();	// Call if not carry
uint8_t CALLNZ();	// Call if not zero
uint8_t CALLZ();	// Call if zero
uint8_t CCF();		// Complement carry flag - flip it
uint8_t CP();		// Compare
uint8_t CPL();		// Flips every bit in A - A = ~A
uint8_t DAA();		// Decimal Adjust Accumulator
uint8_t DECA();		// Decrement A
uint8_t DECB();
uint8_t DECC();
uint8_t DECD();
uint8_t DECE();
uint8_t DECH();
uint8_t DECL();
uint8_t DECHL();	// Decrement (HL)
uint8_t DI();		// Disable interrupts
uint8_t EI();		// Enable interrupts
uint8_t HALT();		// Halt until interrupt occurs
uint8_t INA();		// Increment A
uint8_t INB();
uint8_t INC();
uint8_t IND();
uint8_t INE();
uint8_t INH();
uint8_t INL();
uint8_t INHL();		// Increment (HL)
uint8_t JP();		// Jump to 16-bit address
uint8_t JPC();		// Jump if carry
uint8_t JPHL();		// Jump to (HL)
uint8_t JPNC();		// Jump if not carry
uint8_t JPNZ();		// Jump if not zero
uint8_t JPZ();		// Jump if zero
uint8_t JR();		// Jump relative
uint8_t JRC();		// Jump relative if carry
uint8_t JRNC();		// Jump relative if not carry
uint8_t JRNZ();		// Jump relative if not zero
uint8_t JRZ();		// Jump relative if zero
uint8_t LDA();		// Load A
uint8_t LDB();
uint8_t LDC();
uint8_t LDD();
uint8_t LDE();
uint8_t LDH();
uint8_t LDL();
uint8_t LDBC();		// Load to (BC)
uint8_t LDDE();
uint8_t LDHL(); 
uint8_t LDION();	// Load A to (0xFF00+n)
uint8_t LDIOC();	// Load A to (0xFF00+C)
uint8_t LDAINC();	// Load (HL) to A and HL++
uint8_t LDADEC();	// Load (HL) to A and HL--
uint8_t LDINC();	// Load A to (HL) and HL++
uint8_t LDDEC();	// Load A to (HL) and HL--
uint8_t LDN();		// Load A to (nn)
uint8_t NOP();		// No operation
uint8_t OR();		// Or
uint8_t POPAF();	// Pop AF
uint8_t POPBC();
uint8_t POPDE();
uint8_t POPHL();
uint8_t PRECB();	// All functions with the prefix $CB
uint8_t PUSHAF();	// Push AF
uint8_t PUSHBC();
uint8_t PUSHDE();
uint8_t PUSHHL();
uint8_t RET();		// Return
uint8_t RETC();		// Return if carry
uint8_t RETI();		// Return and enable interrupts
uint8_t RETNC();	// Return if not carry
uint8_t RETNZ();	// Return if not zero
uint8_t RETZ();		// Return if zero
uint8_t RES0();		// Reset bit 0
uint8_t RES1();
uint8_t RES2();
uint8_t RES3();
uint8_t RES4();
uint8_t RES5();
uint8_t RES6();
uint8_t RES7();
uint8_t RLA();		// Rotate left A through carry
uint8_t RL();		// Rotate left through carry
uint8_t RLC();		// Rotate left
uint8_t RLCA();		// Rotate left A
uint8_t RR();		// Rotate right through carry
uint8_t RRA();		// Rotate right A through carry
uint8_t RRC();		// Rotate right
uint8_t RRCA();		// Rotate right A
uint8_t RST00();	// Call to address $00
uint8_t RST08();
uint8_t RST10();
uint8_t RST18();
uint8_t RST20();
uint8_t RST28();
uint8_t RST30();
uint8_t RST38();
uint8_t SBC();		// Subtract with carry
uint8_t SCF();		// Set carry flag
uint8_t SET0();		// Set bit 0
uint8_t SET1();
uint8_t SET2();
uint8_t SET3();
uint8_t SET4();
uint8_t SET5();
uint8_t SET6();
uint8_t SET7();
uint8_t SLA();		// Shift left arithmetic (normal shift left)
uint8_t SRA();		// Shift right arithmetic (b7=b7)
uint8_t SRL();		// Shift right logic (normal shift right)
uint8_t STOP();		// Low power standby mode (very low power) - similar to HALT
uint8_t SUB();		// Subtract
uint8_t SWAP();		// Swap upper 4 bits with lower 4 bits
uint8_t XOR();		// Xor
uint8_t XXX();		// Illegal opcode

// 16-bit Instructions
uint8_t ADDHL_16();	// Add to HL
uint8_t ADDSP_16();	// Add signed 8-bit number to SP
uint8_t DECBC_16();	// Decrement BC
uint8_t DECDE_16();
uint8_t DECHL_16();
uint8_t DECSP_16();
uint8_t INBC_16();	// Increment BC
uint8_t INDE_16();
uint8_t INHL_16();
uint8_t INSP_16();
uint8_t LDBC_16();
uint8_t LDDE_16();
uint8_t LDHL_16();
uint8_t LDHLSP_16();
uint8_t LDSP_16();
uint8_t LDASP_16();	// Load to address from SP

// Instruction data
typedef struct {
	uint8_t(*func)();
	uint8_t(*addrmode)();
	uint8_t cycles;
	// Adding string and length for disassembly
	wchar_t repr[50];
	uint8_t inst_len;
} INSTRUCTION;

// Debugging
// This function gets a pointer to RAM, disassembles the instruction and outputs the disassembled
// string. Returns number of bytes the instruction takes.
uint8_t disassemble_inst(uint16_t inst_pointer, wchar_t* disassembled_inst, int strlen);

// TODO: Disassemble the entire ROM by following the code paths

#endif // CPU_CODE