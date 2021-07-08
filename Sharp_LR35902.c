#include "Bus.h"
#include "Sharp_LR35902.h"

// 8-bit registers
#define A ((uint8_t) (AF >> 8))
#define B ((uint8_t) (BC >> 8))
#define C ((uint8_t) BC)
#define D ((uint8_t) (DE >> 8))
#define E ((uint8_t) DE)
#define H ((uint8_t) (HL >> 8))
#define L ((uint8_t) HL)
#define F ((uint8_t) AF)		// Flags


// Static variables
// Define registers and flags
// 16-bit registers
static uint16_t AF = 0x0000;	// Accumulator and flags
static uint16_t BC = 0x0000;
static uint16_t DE = 0x0000;
static uint16_t HL = 0x0000;
static uint16_t SP = 0x0000;	// Stack Pointer
static uint16_t PC = 0x0000;	// Program Counter

static bool IME = false;			// Interrupt Master Enable flag
static uint8_t enable_int = 0x00;	// Needed for enabling the interrupt one instruction later

static uint8_t  opcode		= 0x00;
uint8_t	cb_opcode	= 0x00;			// This is not static ON PURPOSE! If it's static it causes a weird bug
static uint8_t  cycles		= 0x00;
static uint8_t	fetched		= 0x00;
static uint16_t fetched_16	= 0x0000;


static uint8_t  temp = 0x00;		// Used for many operations
static uint16_t temp_16 = 0x0000;
static uint32_t temp_32 = 0x00000000;

static bool halt_bug = false;		// Used for the HALT bug
static bool halt_occured = false;

uint16_t stack_base = 0x0000;	// For debug purposes
								// I think the only functions that would be used to change the stack base
								// would be LD SP,d16 and LD SP,HL.
								// There are also INC SP, DEC SP and ADD SP,r8, but I think they would only
								// really be used when implementing PUSH and POP by your self.

static INSTRUCTION optable[16 * 16] = {
	{NOP, IMP, 1, L"NOP", 1},					{LDBC_16, IMM_16, 3, L"LD BC,$%04x", 3},	{LDBC, RGA, 2, L"LD (BC),A", 1},		{INBC_16, IMP, 2, L"INC BC", 1},	{INB, IMP, 1, L"INC B", 1},					{DECB, IMP, 1, L"DEC B", 1},		{LDB, IMM, 2, L"LD B,$%02x", 2},		{RLCA, IMP, 1, L"RLC A", 1},		{LDASP_16, IMM_16, 5, L"LD ($%04x),SP", 3},		{ADDHL_16, RGBC_16, 2, L"ADD HL,BC", 1},	{LDA, RGBC, 2, L"LD A,(BC)", 1},		{DECBC_16, IMP, 2, L"DEC BC", 1},	{INC, IMP, 1, L"INC C", 1},				{DECC, IMP, 1, L"DEC C", 1},			{LDC, IMM, 2, L"LD C,$%02x", 2},		{RRCA, IMP, 1, L"RRCA", 1},
	{STOP, IMP, 0XFF, L"STOP", 1},				{LDDE_16, IMM_16, 3, L"LD DE,$%04x", 3},	{LDDE, RGA, 2, L"LD (DE),A", 1},		{INDE_16, IMP, 2, L"INC DE", 1},	{IND, IMP, 1, L"INC D", 1},					{DECD, IMP, 1, L"DEC D", 1},		{LDD, IMM, 2, L"LD D,$%02x", 2},		{RLA, IMP, 1, L"RL A", 1},			{JR, IMM, 3, L"JR $%02x", 2},					{ADDHL_16, RGDE_16, 2, L"ADD HL,DE", 1},	{LDA, RGDE, 2, L"LD A,(DE)", 1},		{DECDE_16, IMP, 2, L"DEC DE", 1},	{INE, IMP, 1, L"INC E", 1},				{DECE, IMP, 1, L"DEC E", 1},			{LDE, IMM, 2, L"LD E,$%02x", 2},		{RRA, IMP, 1, L"RRA", 1},
	{JRNZ, IMM, 2, L"JR NZ,$%02x", 2},			{LDHL_16, IMM_16, 3, L"LD HL,$%04x", 3},	{LDINC, IMP, 2, L"LD (HL+),A", 1},		{INHL_16, IMP, 2, L"INC HL", 1},	{INH, IMP, 1, L"INC H", 1},					{DECH, IMP, 1, L"DEC H", 1},		{LDH, IMM, 2, L"LD H,$%02x", 2},		{DAA, IMP, 1, L"DAA", 1},			{JRZ, IMM, 2, L"JR Z,$%02x", 2},				{ADDHL_16, RGHL_16, 2, L"ADD HL,HL", 1},	{LDAINC, IMP, 2, L"LD A,(HL+)", 1},		{DECHL_16, IMP, 2, L"DEC HL", 1},	{INL, IMP, 1, L"INC L", 1},				{DECL, IMP, 1, L"DEC L", 1},			{LDL, IMM, 2, L"LD L,$%02x", 2},		{CPL, IMP, 1, L"CPL", 1},
	{JRNC, IMM, 2, L"JR NC,$%02x", 2},			{LDSP_16, IMM_16, 3, L"LD SP,$%04x", 3},	{LDDEC, IMP, 2, L"LD (HL-),A", 1},		{INSP_16, IMP, 2, L"INC SP", 1},	{INHL, IMP, 3, L"INC (HL)", 1},				{DECHL, IMP, 3, L"DEC (HL)", 1},	{LDHL, IMM, 3, L"LD (HL),$%02x", 2},	{SCF, IMP, 1, L"SCF", 1},			{JRC, IMM, 2, L"JR C,$%02x", 2},				{ADDHL_16, RGSP_16, 2, L"ADD HL,SP", 1},	{LDADEC, IMP, 2, L"LD A,(HL-)", 1},		{DECSP_16, IMP, 2, L"DEC SP", 1},	{INA, IMP, 1, L"INC A", 1},				{DECA, IMP, 1, L"DEC A", 1},			{LDA, IMM, 2, L"LD A,$%02x", 2},		{CCF, IMP, 1, L"CCF", 1},
	{LDB, RGB, 1, L"LD B,B", 1},				{LDB, RGC, 1, L"LD B,C", 1},				{LDB, RGD, 1, L"LD B,D", 1},			{LDB, RGE, 1, L"LD B,E", 1},		{LDB, RGH, 1, L"LD B,H", 1},				{LDB, RGL, 1, L"LD B,L", 1},		{LDB, RGHL, 2, L"LD B,(HL)", 1},		{LDB, RGA, 1, L"LD B,A", 1},		{LDC, RGB, 1, L"LD C,B", 1},					{LDC, RGC, 1, L"LD C,C", 1},				{LDC, RGD, 1, L"LD C,D", 1},			{LDC, RGE, 1, L"LD C,E", 1},		{LDC, RGH, 1, L"LD C,H", 1},			{LDC, RGL, 1, L"LD C,L", 1},			{LDC, RGHL, 2, L"LD C,(HL)", 1},		{LDC, RGA, 1, L"LD C,A", 1},
	{LDD, RGB, 1, L"LD D,B", 1},				{LDD, RGC, 1, L"LD D,C", 1},				{LDD, RGD, 1, L"LD D,D", 1},			{LDD, RGE, 1, L"LD D,E", 1},		{LDD, RGH, 1, L"LD D,H", 1},				{LDD, RGL, 1, L"LD D,L", 1},		{LDD, RGHL, 2, L"LD D,(HL)", 1},		{LDD, RGA, 1, L"LD D,A", 1},		{LDE, RGB, 1, L"LD E,B", 1},					{LDE, RGC, 1, L"LD E,C", 1},				{LDE, RGD, 1, L"LD E,D", 1},			{LDE, RGE, 1, L"LD E,E", 1},		{LDE, RGH, 1, L"LD E,H", 1},			{LDE, RGL, 1, L"LD E,L", 1},			{LDE, RGHL, 2, L"LD E,(HL)", 1},		{LDE, RGA, 1, L"LD E,A", 1},
	{LDH, RGB, 1, L"LD H,B", 1},				{LDH, RGC, 1, L"LD H,C", 1},				{LDH, RGD, 1, L"LD H,D", 1},			{LDH, RGE, 1, L"LD H,E", 1},		{LDH, RGH, 1, L"LD H,H", 1},				{LDH, RGL, 1, L"LD H,L", 1},		{LDH, RGHL, 2, L"LD H,(HL)", 1},		{LDH, RGA, 1, L"LD H,A", 1},		{LDL, RGB, 1, L"LD L,B", 1},					{LDL, RGC, 1, L"LD L,C", 1},				{LDL, RGD, 1, L"LD L,D", 1},			{LDL, RGE, 1, L"LD L,E", 1},		{LDL, RGH, 1, L"LD L,H", 1},			{LDL, RGL, 1, L"LD L,L", 1},			{LDL, RGHL, 2, L"LD L,(HL)", 1},		{LDL, RGA, 1, L"LD L,A", 1},
	{LDHL, RGB, 2, L"LD (HL),B", 1},			{LDHL, RGC, 2, L"LD (HL),C", 1},			{LDHL, RGD, 2, L"LD (HL),D", 1},		{LDHL, RGE, 2, L"LD (HL),E", 1},	{LDHL, RGH, 2, L"LD (HL),H", 1},			{LDHL, RGL, 2, L"LD (HL),L", 1},	{HALT, IMP, 1, L"HALT", 1},				{LDHL, RGA, 2, L"LD (HL),A", 1},	{LDA, RGB, 1, L"LD A,B", 1},					{LDA, RGC, 1, L"LD A,C", 1},				{LDA, RGD, 1, L"LD A,D", 1},			{LDA, RGE, 1, L"LD A,E", 1},		{LDA, RGH, 1, L"LD A,H", 1},			{LDA, RGL, 1, L"LD A,L", 1},			{LDA, RGHL, 2, L"LD A,(HL)", 1},		{LDA, RGA, 1, L"LD A,A", 1},
	{ADD, RGB, 1, L"ADD A,B", 1},				{ADD, RGC, 1, L"ADD A,C", 1},				{ADD, RGD, 1, L"ADD A,D", 1},			{ADD, RGE, 1, L"ADD A,E", 1},		{ADD, RGH, 1, L"ADD A,H", 1},				{ADD, RGL, 1, L"ADD A,L", 1},		{ADD, RGHL, 2, L"ADD A,(HL)", 1},		{ADD, RGA, 1, L"ADD A,A", 1},		{ADC, RGB, 1, L"ADC A,B", 1},					{ADC, RGC, 1, L"ADC A,C", 1},				{ADC, RGD, 1, L"ADC A,D", 1},			{ADC, RGE, 1, L"ADC A,E", 1},		{ADC, RGH, 1, L"ADC A,H", 1},			{ADC, RGL, 1, L"ADC A,L", 1},			{ADC, RGHL, 2, L"ADC A,(HL)", 1},		{ADC, RGA, 1, L"ADC A,A", 1},
	{SUB, RGB, 1, L"SUB B", 1},					{SUB, RGC, 1, L"SUB C", 1},					{SUB, RGD, 1, L"SUB D", 1},				{SUB, RGE, 1, L"SUB E", 1},			{SUB, RGH, 1, L"SUB H", 1},					{SUB, RGL, 1, L"SUB L", 1},			{SUB, RGHL, 2, L"SUB (HL)", 1},			{SUB, RGA, 1, L"SUB A", 1},			{SBC, RGB, 1, L"SBC A,B", 1},					{SBC, RGC, 1, L"SBC A,C", 1},				{SBC, RGD, 1, L"SBC A,D", 1},			{SBC, RGE, 1, L"SBC A,E", 1},		{SBC, RGH, 1, L"SBC A,H", 1},			{SBC, RGL, 1, L"SBC A,L", 1},			{SBC, RGHL, 2, L"SBC A,(HL)", 1},		{SBC, RGA, 1, L"SBC A,A", 1},
	{AND, RGB, 1, L"AND B", 1},					{AND, RGC, 1, L"AND C", 1},					{AND, RGD, 1, L"AND D", 1},				{AND, RGE, 1, L"AND E", 1},			{AND, RGH, 1, L"AND H", 1},					{AND, RGL, 1, L"AND L", 1},			{AND, RGHL, 2, L"AND (HL)", 1},			{AND, RGA, 1, L"AND A", 1},			{XOR, RGB, 1, L"XOR B", 1},						{XOR, RGC, 1, L"XOR C", 1},					{XOR, RGD, 1, L"XOR D", 1},				{XOR, RGE, 1, L"XOR E", 1},			{XOR, RGH, 1, L"XOR H", 1},				{XOR, RGL, 1, L"XOR L", 1},				{XOR, RGHL, 2, L"XOR (HL)", 1},			{XOR, RGA, 1, L"XOR A", 1},
	{OR, RGB, 1, L"OR B", 1},					{OR, RGC, 1, L"OR C", 1},					{OR, RGD, 1, L"OR D", 1},				{OR, RGE, 1, L"OR E", 1},			{OR, RGH, 1, L"OR H", 1},					{OR, RGL, 1, L"OR L", 1},			{OR, RGHL, 2, L"OR (HL)", 1},			{OR, RGA, 1, L"OR A", 1},			{CP, RGB, 1, L"CP B", 1},						{CP, RGC, 1, L"CP C", 1},					{CP, RGD, 1, L"CP D", 1},				{CP, RGE, 1, L"CP E", 1},			{CP, RGH, 1, L"CP H", 1},				{CP, RGL, 1, L"CP L", 1},				{CP, RGHL, 2, L"CP (HL)", 1},			{CP, RGA, 1, L"CP A", 1},
	{RETNZ, IMP, 2, L"RET NZ", 1},				{POPBC, IMP, 3, L"POP BC", 1},				{JPNZ, IMM_16, 3, L"JP NZ,$%04x", 3},	{JP, IMM_16, 4, L"JP $%04x", 3},	{CALLNZ, IMM_16, 3, L"CALL NZ,$%04x", 3},	{PUSHBC, IMP, 4, L"PUSH BC", 1},	{ADD, IMM, 2, L"ADD A,$%02x", 2},		{RST00, IMP, 4, L"RST 00", 1},		{RETZ, IMP, 2, L"RET Z", 1},					{RET, IMP, 4, L"RET", 1},					{JPZ, IMM_16, 3, L"JP Z,$%04x", 3},		{PRECB, IMM, 2, L"PREFIX", 2},		{CALLZ, IMM_16, 3, L"CALL Z,$%04x", 3},	{CALL, IMM_16, 6, L"CALL $%04x", 3},	{ADC, IMM, 2, L"ADC A,$%02x", 2},		{RST08, IMP, 4, L"RST 08", 1},
	{RETNC, IMP, 2, L"RET NC", 1},				{POPDE, IMP, 3, L"POP DE", 1},				{JPNC, IMM_16, 3, L"JP NC,$%04x", 3},	{XXX, IMP, 1, L"???", 1},			{CALLNC, IMM_16, 3, L"CALL NC,$%04x", 3},	{PUSHDE, IMP, 4, L"PUSH DE", 1},	{SUB, IMM, 2, L"SUB $%02x", 2 },		{RST10, IMP, 4, L"RST 10", 1},		{RETC, IMP, 2, L"RET C", 1},					{RETI, IMP, 4, L"RETI", 1},					{JPC, IMM_16, 3, L"JP C,$%04x", 3},		{XXX, IMP, 1, L"???", 1},			{CALLC, IMM_16, 3, L"CALL C,$%04x", 3},	{XXX, IMP, 1, L"???", 1},				{SBC, IMM, 2, L"SBC A,$%02x", 2},		{RST18, IMP, 4, L"RST 18", 1},
	{LDION, IMM, 3, L"LD ($ff00+$%02x),A", 2},	{POPHL, IMP, 3, L"POP HL", 1},				{LDIOC, RGA, 2, L"LD ($ff00+C),A", 1},	{XXX, IMP, 1, L"???", 1},			{XXX, IMP, 1, L"???", 1},					{PUSHHL, IMP, 4, L"PUSH HL", 1},	{AND, IMM, 2, L"AND $%02x", 2},			{RST20, IMP, 4, L"RST 20", 1},		{ADDSP_16, IMM, 4, L"ADD SP,$%02x", 2},			{JPHL, IMP, 1, L"JP HL", 1},				{LDN, IMM_16, 4, L"LD ($%04x),A", 3},	{XXX, IMP, 1, L"???", 1},			{XXX, IMP, 1, L"???", 1},				{XXX, IMP, 1, L"???", 1},				{XOR, IMM, 2, L"XOR $%02x", 2},			{RST28, IMP, 4, L"RST 28", 1},
	{LDA, PTRIO, 3, L"LD A,($ff00+$%02x)", 2},	{POPAF, IMP, 3, L"POP AF", 1},				{LDA, RGCIO, 2, L"LD A,($ff00+C)", 1},	{DI, IMP, 1, L"DI", 1},				{XXX, IMP, 1, L"???", 1},					{PUSHAF, IMP, 4, L"PUSH AF", 1},	{OR, IMM, 2, L"OR $%02x", 2},			{RST30, IMP, 4, L"RST 30", 1},		{LDHLSP_16, IMM, 3, L"LD HL,SP+$%02x", 2},		{LDSP_16, RGHL_16, 2, L"LD SP,HL", 1},		{LDA, PTR, 4, L"LD A,($%04x)", 3},		{EI, IMP, 1, L"EI", 1},				{XXX, IMP, 1, L"???", 1},				{XXX, IMP, 1, L"???", 1},				{CP, IMM, 2, L"CP $%02x", 2},			{RST38, IMP, 4, L"RST 38", 1}
};
static uint8_t(*precbtable[32])() = {
	RLC,  RRC,  RL,   RR,   SLA,  SRA,  SWAP, SRL,  BIT0, BIT1, BIT2, BIT3, BIT4, BIT5, BIT6, BIT7,
	RES0, RES1, RES2, RES3, RES4, RES5, RES6, RES7, SET0, SET1, SET2, SET3, SET4, SET5, SET6, SET7
};
static wchar_t dis_precb[32][15] = {
	L"RLC %s", L"RRC %s", L"RL %s", L"RR %s", L"SLA %s", L"SRA %s", L"SWAP %s", L"SRL %s", L"BIT 0,%s", L"BIT 1,%s", L"BIT 2,%s", L"BIT 3,%s", L"BIT 4,%s", L"BIT 5,%s", L"BIT 6,%s", L"BIT 7,%s",
	L"RES 0,%s", L"RES 1,%s", L"RES 2,%s", L"RES 3,%s", L"RES 4,%s", L"RES 5,%s", L"RES 6,%s", L"RES 7,%s", L"SET 0,%s", L"SET 1,%s", L"SET 2,%s", L"SET 3,%s", L"SET 4,%s", L"SET 5,%s", L"SET 6,%s", L"SET 7,%s"
};


uint8_t cpu_getflag(FLAGSLR35902 f)
{
	return (F & f) > 0 ? 1 : 0;
}

void cpu_setflag(FLAGSLR35902 f, bool b)
{
	if (b)
		AF |= f;
	else
		AF &= ~f;
}

void cpu_set_a(uint8_t data) 
{
	AF = ((uint16_t)data) << 8 | F;
}

void cpu_set_b(uint8_t data)
{
	BC = ((uint16_t)data) << 8 | C;
}

void cpu_set_c(uint8_t data)
{
	BC = (BC & 0xFF00) | data;
}

void cpu_set_d(uint8_t data)
{
	DE = ((uint16_t)data) << 8 | E;
}

void cpu_set_e(uint8_t data)
{
	DE = (DE & 0xFF00) | data;
}

void cpu_set_h(uint8_t data)
{
	HL = ((uint16_t)data) << 8 | L;
}

void cpu_set_l(uint8_t data)
{
	HL = (HL & 0xFF00) | data;
}

void cpu_set_f(uint8_t data)
{
	// Only affects flags
	AF = (AF & 0xFF00) | (data & 0xF0);
}

void cpu_clock()
{
	// Interrupt check routine
	if (cpu_int_check)
	{
		cpu_int_check = false;
		temp = cpu_read(IF_ADDR);
		uint8_t temp2 = cpu_read(IE_ADDR);
		if (IME)
		{
			if (temp & temp2)
			{
				if (halt_occured)
					halt_occured = false;

				// Check each interrupt starting from the 0 bit
				if (temp & INT_VBLANK & temp2)
				{
					// Clear bit
					cpu_write(IF_ADDR, temp & ~INT_VBLANK);

					// Disable IME
					enable_int = INT_DISABLE;

					// Call VBlank routine
					// Push PC to the stack
					cpu_write(--SP, (uint8_t)(PC >> 8));
					cpu_write(--SP, (uint8_t)PC);

					// Jump to VBlank routine
					PC = INT_VBLANK_RTN;

					// This whole operation should take 5 cycles
					cycles += 5;
					return;
				}
				else if (temp & INT_LCDSTAT & temp2)
				{
					cpu_write(IF_ADDR, temp & ~INT_LCDSTAT);
					enable_int = INT_DISABLE;
					cpu_write(--SP, (uint8_t)(PC >> 8));
					cpu_write(--SP, (uint8_t)PC);
					PC = INT_LEDSTAT_RTN;
					cycles += 5;
					return;
				}
				else if (temp & INT_TIMER & temp2)
				{
					cpu_write(IF_ADDR, temp & ~INT_TIMER);
					enable_int = INT_DISABLE;
					cpu_write(--SP, (uint8_t)(PC >> 8));
					cpu_write(--SP, (uint8_t)PC);
					PC = INT_TIMER_RTN;
					cycles += 5;
					return;
				}
				else if (temp & INT_SERIAL & temp2)
				{
					cpu_write(IF_ADDR, temp & ~INT_SERIAL);
					enable_int = INT_DISABLE;
					cpu_write(--SP, (uint8_t)(PC >> 8));
					cpu_write(--SP, (uint8_t)PC);
					PC = INT_SERIAL_RTN;
					cycles += 5;
					return;
				}
				else if (temp & INT_JOYPAD & temp2)
				{
					cpu_write(IF_ADDR, temp & ~INT_JOYPAD);
					enable_int = INT_DISABLE;
					cpu_write(--SP, (uint8_t)(PC >> 8));
					cpu_write(--SP, (uint8_t)PC);
					PC = INT_JOYPAD_RTN;
					cycles += 5;
					return;
				}
			}
		}
		else
		{
			if (temp & temp2)
			{
				// Halt bug
				if (halt_occured)
				{
					halt_occured = false;
					halt_bug = true;
				}
			}
			else if (halt_occured)
				cpu_halt = true;

			return;
		}
	}
	if (cycles == 0) 
	{
		

		opcode = cpu_read(PC);

		// Halt bug - the PC doesn't progress once after the HALT instruction
		if (!halt_bug)
			PC++;
		else
			halt_bug = false;

		cycles = optable[opcode].cycles - 1; // This is the first cycle so only n-1 left
		(*optable[opcode].addrmode)();
		(*optable[opcode].func)();

		

		// Make sure that when enabling interrupts it only enables after the next instruction
		if (enable_int == INT_ENABLE)
		{
			IME = true;
			enable_int--;
		}
		else if (enable_int == INT_DELAY_ENABLE)
			enable_int--;
		else if (enable_int == INT_DISABLE)
		{
			IME = false;
			enable_int = INT_NO_CHANGE;
		}

	}
	else
	{
		cycles--;
	}
}

void cpu_reset(bool bootskip)
{
	PC = 0x0000;	// Program counter resets to the boot
	SP = 0x0000;
	stack_base = SP;

	AF = 0x0000;
	BC = 0X0000;
	DE = 0x0000;
	HL = 0x0000;

	IME = false;
	enable_int = INT_NO_CHANGE;
	
	halt_bug = false;
	halt_occured = false;

	// For testing purposes
	if (bootskip)
	{
		PC = 0x0100;
		SP = 0xFFFE;	// Zero page is intended for use as stack

		AF = 0x01B0;
		BC = 0x0013;
		DE = 0x00D8;
		HL = 0x014D;
	}
}

uint8_t cpu_read(uint16_t addr)
{
	return bus_read(addr, DEV_CPU);
}

void cpu_write(uint16_t addr, uint8_t data)
{
	bus_write(addr, data, DEV_CPU);
}

void cpu_get_stats(uint16_t* af, uint16_t* bc, uint16_t* de, uint16_t* hl, uint16_t* sp, uint16_t* pc, 
	bool* ime, uint8_t* stat_opcode, uint8_t* stat_cycles, uint8_t* stat_fetched, uint16_t* stat_fetched16, 
	uint8_t* cb_op, uint16_t* stat_stackbase)
{
	if (af != NULL)
		*af = AF;
	if (bc != NULL)
		*bc = BC;
	if (de != NULL)
		*de = DE;
	if (hl != NULL)
		*hl = HL;
	if (sp != NULL)
		*sp = SP;
	if (pc != NULL)
		*pc = PC;
	if (ime != NULL)
		*ime = IME;
	if (stat_opcode != NULL)
		*stat_opcode = opcode;
	if (stat_cycles != NULL)
		*stat_cycles = cycles;
	if (stat_fetched != NULL)
		*stat_fetched = fetched;
	if (stat_fetched16 != NULL)
		*stat_fetched16 = fetched_16;
	if (cb_op != NULL)
		*cb_op = cb_opcode;
	if (stat_stackbase != NULL)
		*stat_stackbase = stack_base;
}


// 8-bit Address modes
uint8_t IMP()
{
	return 0;
}

uint8_t IMM()
{
	fetched = cpu_read(PC++);
	return 0;
}

uint8_t PTR()
{
	uint16_t addr_lo = (uint16_t) cpu_read(PC++);
	uint16_t addr_hi = (uint16_t) cpu_read(PC++);

	uint16_t addr = (addr_hi << 8) | addr_lo;
	fetched = cpu_read(addr);
	return 0;
}

uint8_t PTRIO()
{
	// ($FF00+n)
	// read from address $FF00+<next byte>
	uint16_t addr = 0xFF00 | cpu_read(PC++);

	fetched = cpu_read(addr);
	return 0;
}

uint8_t RGA()
{
	fetched = A;
	return 0;
}

uint8_t RGB()
{
	fetched = B;
	return 0;
}

uint8_t RGC()
{
	fetched = C;
	return 0;
}

uint8_t RGD()
{
	fetched = D;
	return 0;
}

uint8_t RGE()
{
	fetched = E;
	return 0;
}

uint8_t RGH()
{
	fetched = H;
	return 0;
}

uint8_t RGL()
{
	fetched = L;
	return 0;
}

uint8_t RGBC()
{
	fetched = cpu_read(BC);
	return 0;
}

uint8_t RGDE()
{
	fetched = cpu_read(DE);
	return 0;
}

uint8_t RGHL()
{
	fetched = cpu_read(HL);
	return 0;
}

uint8_t RGCIO()
{
	// ($FF00+C)
	// read from address $FF00+C
	fetched = cpu_read(0xFF00|C);
	return 0;
}

uint8_t IMM_16()
{
	uint16_t lo = cpu_read(PC++);
	uint16_t hi = cpu_read(PC++);
	fetched_16 = (hi << 8) | lo;
	return 0;
}

uint8_t RGBC_16()
{
	fetched_16 = BC;
	return 0;
}

uint8_t RGDE_16()
{
	fetched_16 = DE;
	return 0;
}

uint8_t RGHL_16()
{
	fetched_16 = HL;
	return 0;
}

uint8_t RGSP_16()
{
	fetched_16 = SP;
	return 0;
}

uint8_t SPSN_16()
{
	fetched_16 = SP + (int8_t) cpu_read(PC++);
	return 0;
}

// 8-bit Instructions

uint8_t ADC()
{
	
	temp_16 = A + fetched + cpu_getflag(Cy);


	// Set flags
	cpu_setflag(Z, (temp_16 & 0x00FF) == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, (((0x0F & A) + (0x0F & fetched) + cpu_getflag(Cy)) & 0x10) > 0);
	cpu_setflag(Cy, (0x0100 & temp_16) > 0);

	cpu_set_a((uint8_t)temp_16);
	return 0;
};

uint8_t ADD()
{
	temp_16 = A + fetched;
	

	// Set flags
	cpu_setflag(Z, (temp_16 & 0x00FF) == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, (((0x0F & A) + (0x0F & fetched)) & 0x10) > 0);
	cpu_setflag(Cy, (0x0100 & temp_16) > 0);

	cpu_set_a((uint8_t)temp_16);
	return 0;
};

uint8_t AND()
{
	cpu_set_a(A & fetched);

	// Set flags
	cpu_setflag(Z, ((A & fetched) == 0));
	cpu_setflag(N, false);
	cpu_setflag(Hc, true);
	cpu_setflag(Cy, false);
	return 0;
};

uint8_t BIT0()
{
	// Set flags
	cpu_setflag(Z, (fetched & (1 << 0)) == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, true);
	return 0;
};

uint8_t BIT1()
{
	// Set flags
	cpu_setflag(Z, (fetched & (1 << 1)) == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, true);
	return 0;
};

uint8_t BIT2()
{
	// Set flags
	cpu_setflag(Z, (fetched & (1 << 2)) == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, true);
	return 0;
};

uint8_t BIT3()
{
	// Set flags
	cpu_setflag(Z, (fetched & (1 << 3)) == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, true);
	return 0;
};

uint8_t BIT4()
{
	// Set flags
	cpu_setflag(Z, (fetched & (1 << 4)) == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, true);
	return 0;
};

uint8_t BIT5()
{
	// Set flags
	cpu_setflag(Z, (fetched & (1 << 5)) == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, true);
	return 0;
};

uint8_t BIT6()
{
	// Set flags
	cpu_setflag(Z, (fetched & (1 << 6)) == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, true);
	return 0;
};

uint8_t BIT7()
{
	// Set flags
	cpu_setflag(Z, (fetched & (1 << 7)) == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, true);
	return 0;
};

uint8_t CALL()
{
	cpu_write(--SP, (uint8_t)(PC >> 8));
	cpu_write(--SP, (uint8_t)PC);
	PC = fetched_16;
	return 0;
};

uint8_t CALLC()
{
	if ((bool)cpu_getflag(Cy)) 
	{
		cpu_write(--SP, (uint8_t)(PC >> 8));
		cpu_write(--SP, (uint8_t)PC);
		PC = fetched_16;
		cycles += 3;
	}
	return 0;
};

uint8_t CALLNC()
{
	if (!(bool)cpu_getflag(Cy))
	{
		cpu_write(--SP, (uint8_t)(PC >> 8));
		cpu_write(--SP, (uint8_t)PC);
		PC = fetched_16;
		cycles += 3;
	}
	return 0;
};

uint8_t CALLNZ()
{
	if (!(bool)cpu_getflag(Z))
	{
		cpu_write(--SP, (uint8_t)(PC >> 8));
		cpu_write(--SP, (uint8_t)PC);
		PC = fetched_16;
		cycles += 3;
	}
	return 0;
};

uint8_t CALLZ()
{
	if ((bool)cpu_getflag(Z))
	{
		cpu_write(--SP, (uint8_t)(PC >> 8));
		cpu_write(--SP, (uint8_t)PC);
		PC = fetched_16;
		cycles += 3;
	}
	return 0;
};

// Complement carry flag
uint8_t CCF()
{
	// Set flags
	cpu_setflag(Cy, !(bool)cpu_getflag(Cy));
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	return 0;
};

// Compare
uint8_t CP()
{
	// Set flags
	cpu_setflag(Z, A == fetched);
	cpu_setflag(N, true);
	cpu_setflag(Hc, (fetched & 0x0F) > (A & 0x0F));
	cpu_setflag(Cy, fetched > A);
	return 0;
};

// Complement (accumulator)
uint8_t CPL()
{
	cpu_set_a(~A);

	// Set flags
	cpu_setflag(N, true);
	cpu_setflag(Hc, true);
	return 0;
};

// Decimal Adjust Accumulator
// BCD - Binary-Coded Decimal. What does this mean?
// BCD is a representation of decimal numbers in binary form, where patterns of 4 or 8 bits
// represent a decimal number from 0-9.
// In the GameBoy CPU, every decimal digit is represented in 4 bits, meaning that the hexadecimal
// value ranges from 0x0 - 0x9 for each 4 bits, and 0x00 - 0x99 for one byte.
// The DAA function adjusts the contents of the A register after an addition or subtraction of
// 2 BCD numbers, so that the output remains in BCD form.
uint8_t DAA()
{
	if (!cpu_getflag(N))
	{
		if (cpu_getflag(Cy) || A > 0x99)
		{
			cpu_set_a(A + 0x60);
			cpu_setflag(Cy, true);
		}
		if (cpu_getflag(Hc) || (A & 0x0F) > 9)
			cpu_set_a(A + 6);
	}
	else
	{
		if (cpu_getflag(Cy))
			cpu_set_a(A - 0x60);
		if (cpu_getflag(Hc))
			cpu_set_a(A - 6);
	}

	// Set flags
	cpu_setflag(Z, A == 0);
	cpu_setflag(Hc, false);
	return 0;
}

// Decrement A
uint8_t DECA()
{
	// Set half-carry
	cpu_setflag(Hc, (A & 0x0F) == 0);
	cpu_set_a(A - 1);

	// Set flags
	cpu_setflag(Z, A == 0);
	cpu_setflag(N, true);
	return 0;
};

uint8_t DECB()
{
	// Set half-carry
	cpu_setflag(Hc, (B & 0x0F) == 0);
	cpu_set_b(B - 1);

	// Set flags
	cpu_setflag(Z, B == 0);
	cpu_setflag(N, true);
	return 0;
};

uint8_t DECC()
{
	// Set half-carry
	cpu_setflag(Hc, (C & 0x0F) == 0);
	cpu_set_c(C - 1);

	// Set flags
	cpu_setflag(Z, C == 0);
	cpu_setflag(N, true);
	return 0;
};

uint8_t DECD()
{
	// Set half-carry
	cpu_setflag(Hc, (D & 0x0F) == 0);
	cpu_set_d(D - 1);

	// Set flags
	cpu_setflag(Z, D == 0);
	cpu_setflag(N, true);
	return 0;
};

uint8_t DECE()
{
	// Set half-carry
	cpu_setflag(Hc, (E & 0x0F) == 0);
	cpu_set_e(E - 1);

	// Set flags
	cpu_setflag(Z, E == 0);
	cpu_setflag(N, true);
	return 0;
};

uint8_t DECH()
{
	// Set half-carry
	cpu_setflag(Hc, (H & 0x0F) == 0);
	cpu_set_h(H - 1);

	// Set flags
	cpu_setflag(Z, H == 0);
	cpu_setflag(N, true);
	return 0;
};

uint8_t DECL()
{
	// Set half-carry
	cpu_setflag(Hc, (L & 0x0F) == 0);
	cpu_set_l(L - 1);

	// Set flags
	cpu_setflag(Z, L == 0);
	cpu_setflag(N, true);
	return 0;
};

uint8_t DECHL()
{
	temp = cpu_read(HL);

	// Set half-carry
	cpu_setflag(Hc, (temp & 0x0F) == 0);
	cpu_write(HL, temp - 1);

	// Set flags
	cpu_setflag(Z, temp == 1);
	cpu_setflag(N, true);
	return 0;
};

// Disable interrupts
uint8_t DI()
{
	enable_int = INT_DISABLE;
	return 0;
};

// Enable interrupts
uint8_t EI()
{
	enable_int = INT_DELAY_ENABLE;
	return 0;
};

uint8_t HALT()
{
	cpu_halt = true;
	return 0;
};

// Increment A
uint8_t INA()
{
	// Set half carry
	cpu_setflag(Hc, (A & 0x0F) == 0x0F);
	cpu_set_a(A + 1);

	// Set flags
	cpu_setflag(Z, A == 0);
	cpu_setflag(N, false);
	return 0;
};

uint8_t INB()
{
	// Set half carry
	cpu_setflag(Hc, (B & 0x0F) == 0x0F);
	cpu_set_b(B + 1);

	// Set flags
	cpu_setflag(Z, B == 0);
	cpu_setflag(N, false);
	return 0;
};

uint8_t INC()
{
	// Set half carry
	cpu_setflag(Hc, (C & 0x0F) == 0x0F);
	cpu_set_c(C + 1);

	// Set flags
	cpu_setflag(Z, C == 0);
	cpu_setflag(N, false);
	return 0;
};

uint8_t IND()
{
	// Set half carry
	cpu_setflag(Hc, (D & 0x0F) == 0x0F);
	cpu_set_d(D + 1);

	// Set flags
	cpu_setflag(Z, D == 0);
	cpu_setflag(N, false);
	return 0;
};

uint8_t INE()
{
	// Set half carry
	cpu_setflag(Hc, (E & 0x0F) == 0x0F);
	cpu_set_e(E + 1);

	// Set flags
	cpu_setflag(Z, E == 0);
	cpu_setflag(N, false);
	return 0;
};

uint8_t INH()
{
	// Set half carry
	cpu_setflag(Hc, (H & 0x0F) == 0x0F);
	cpu_set_h(H + 1);

	// Set flags
	cpu_setflag(Z, H == 0);
	cpu_setflag(N, false);
	return 0;
};

uint8_t INL()
{
	// Set half carry
	cpu_setflag(Hc, (L & 0x0F) == 0x0F);
	cpu_set_l(L + 1);

	// Set flags
	cpu_setflag(Z, L == 0);
	cpu_setflag(N, false);
	return 0;
};

uint8_t INHL()
{
	temp = cpu_read(HL);

	// Set half carry
	cpu_setflag(Hc, (temp & 0x0F) == 0x0F);
	cpu_write(HL, temp + 1);

	// Set flags
	cpu_setflag(Z, cpu_read(HL) == 0);
	cpu_setflag(N, false);
	return 0;
};

// Jump
uint8_t JP()
{
	PC = fetched_16;
	return 0;
};

// Jump if carry
uint8_t JPC()
{
	if ((bool)cpu_getflag(Cy))
	{
		PC = fetched_16;
		cycles++;
	}
	return 0;
};

uint8_t JPHL()
{
	PC = HL;
	return 0;
};

uint8_t JPNC()
{
	if (!(bool)cpu_getflag(Cy))
	{
		PC = fetched_16;
		cycles++;
	}
	return 0;
};

uint8_t JPNZ()
{
	if (!(bool)cpu_getflag(Z))
	{
		PC = fetched_16;
		cycles++;
	}
	return 0;
};

uint8_t JPZ()
{
	if ((bool)cpu_getflag(Z))
	{
		PC = fetched_16;
		cycles++;
	}
	return 0;
};

// Jump relative
uint8_t JR()
{
	PC += (int8_t) fetched;
	return 0;
};

// Jump relative if carry
uint8_t JRC()
{
	if (cpu_getflag(Cy))
	{
		PC += (int8_t) fetched;
		cycles++;
	}
	return 0;
};

uint8_t JRNC()
{
	if (!cpu_getflag(Cy))
	{
		PC += (int8_t)fetched;
		cycles++;
	}
	return 0;
};

uint8_t JRNZ()
{
	if (!cpu_getflag(Z))
	{
		PC += (int8_t)fetched;
		cycles++;
	}
	return 0;
};

uint8_t JRZ()
{
	if (cpu_getflag(Z))
	{
		PC += (int8_t)fetched;
		cycles++;
	}
	return 0;
};

// Load A
uint8_t LDA()
{
	cpu_set_a(fetched);
	return 0;
}

// Load B
uint8_t LDB()
{
	cpu_set_b(fetched);
	return 0;
}

// Load C
uint8_t LDC()
{
	cpu_set_c(fetched);
	return 0;
}

// Load D
uint8_t LDD()
{
	cpu_set_d(fetched);
	return 0;
}

// Load E
uint8_t LDE()
{
	cpu_set_e(fetched);
	return 0;
}

// Load H
uint8_t LDH()
{
	cpu_set_h(fetched);
	return 0;
}

// Load L
uint8_t LDL()
{
	cpu_set_l(fetched);
	return 0;
}

// Load (BC)
uint8_t LDBC()
{
	cpu_write(BC, fetched);
	return 0;
}

// Load (DE)
uint8_t LDDE()
{
	cpu_write(DE, fetched);
	return 0;
}

// Load (HL)
uint8_t LDHL()
{
	cpu_write(HL, fetched);
	return 0;
}

// Write to IO-port n
uint8_t LDION()
{
	// Load A into ($FF00+n)
	cpu_write(0xFF00|fetched, A);
	return 0;
}

// Write to IO-port C
uint8_t LDIOC()
{
	// Load A into ($FF00+C)
	cpu_write(0xFF00|C, A);
	return 0;
}

// Load (HL) to A and HL++
uint8_t LDAINC()
{
	cpu_set_a(cpu_read(HL++));
	return 0;
}

// Load (HL) to A and HL--
uint8_t LDADEC()
{
	cpu_set_a(cpu_read(HL--));
	return 0;
}

// Load A to (HL) and HL++
uint8_t LDINC()
{
	cpu_write(HL++, A);
	return 0;
}

// Load A to (HL) and HL--
uint8_t LDDEC()
{
	cpu_write(HL--, A);
	return 0;
}

// Load to (nn) from A
uint8_t LDN()
{
	cpu_write(fetched_16, A);
	return 0;
}

// No operation
uint8_t NOP()
{
	// Do nothing
	return 0;
}

uint8_t OR()
{
	cpu_set_a(A | fetched);

	// Set flags
	cpu_setflag(Z, A == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	cpu_setflag(Cy, false);
	return 0;
};

// Pop AF
uint8_t POPAF()
{
	cpu_set_f(cpu_read(SP++));
	cpu_set_a(cpu_read(SP++));
	return 0;
};

uint8_t POPBC()
{
	cpu_set_c(cpu_read(SP++));
	cpu_set_b(cpu_read(SP++));
	return 0;
};

uint8_t POPDE()
{
	cpu_set_e(cpu_read(SP++));
	cpu_set_d(cpu_read(SP++));
	return 0;
};

uint8_t POPHL()
{
	cpu_set_l(cpu_read(SP++));
	cpu_set_h(cpu_read(SP++));
	return 0;
};

// There are a lot of simple instructions that start with $CB
uint8_t PRECB()
{	
	cb_opcode = fetched;

	// Fetch for BIT instructions
	switch (cb_opcode & 0x07) 
	{
		case 0:
			RGB();
			break;
		case 1:
			RGC();
			break;
		case 2:
			RGD();
			break;
		case 3:
			RGE();
			break;
		case 4:
			RGH();
			break;
		case 5:
			RGL();
			break;
		case 6:
			RGHL();
			cycles++;
			break;
		case 7:
			RGA();
			break;
	}

	if (cb_opcode >= 0x40 && cb_opcode < 0x80)
		cycles++;

	// Instruction
	precbtable[(cb_opcode & 0xF8) >> 3]();
	return 0;
};

// Push AF
uint8_t PUSHAF()
{
	cpu_write(--SP, A);
	cpu_write(--SP, F);
	return 0;
};

uint8_t PUSHBC()
{
	cpu_write(--SP, B);
	cpu_write(--SP, C);
	return 0;
};

uint8_t PUSHDE()
{
	cpu_write(--SP, D);
	cpu_write(--SP, E);
	return 0;
};

uint8_t PUSHHL()
{
	cpu_write(--SP, H);
	cpu_write(--SP, L);
	return 0;
};

// Return
uint8_t RET()
{
	PC = (uint16_t) cpu_read(SP++);
	PC = PC | ((uint16_t)cpu_read(SP++) << 8);
	return 0;
};

// Return if carry
uint8_t RETC()
{
	if ((bool)cpu_getflag(Cy))
	{
		PC = (uint16_t)cpu_read(SP++);
		PC = PC | ((uint16_t)cpu_read(SP++) << 8);
		cycles += 3;
	}
	return 0;
};

uint8_t RETI()
{
	PC = (uint16_t)cpu_read(SP++);
	PC = PC | ((uint16_t)cpu_read(SP++) << 8);
	enable_int = INT_ENABLE;
	return 0;
};

uint8_t RETNC()
{
	if (!(bool)cpu_getflag(Cy))
	{
		PC = (uint16_t)cpu_read(SP++);
		PC = PC | ((uint16_t)cpu_read(SP++) << 8);
		cycles += 3;
	}
	return 0;
};

uint8_t RETNZ()
{
	if (!(bool)cpu_getflag(Z))
	{
		PC = (uint16_t)cpu_read(SP++);
		PC = PC | ((uint16_t)cpu_read(SP++) << 8);
		cycles += 3;
	}
	return 0;
};

uint8_t RETZ()
{
	if ((bool)cpu_getflag(Z))
	{
		PC = (uint16_t)cpu_read(SP++);
		PC = PC | ((uint16_t)cpu_read(SP++) << 8);
		cycles += 3;
	}
	return 0;
};

uint8_t RES0()
{
	temp = ~(1 << 0);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B & temp);
		break;
	case 1:
		cpu_set_c(C & temp);
		break;
	case 2:
		cpu_set_d(D & temp);
		break;
	case 3:
		cpu_set_e(E & temp);
		break;
	case 4:
		cpu_set_h(H & temp);
		break;
	case 5:
		cpu_set_l(L & temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) & temp);
		break;
	case 7:
		cpu_set_a(A & temp);
		break;
	}
	return 0;
};

uint8_t RES1()
{
	temp = ~(1 << 1);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B & temp);
		break;
	case 1:
		cpu_set_c(C & temp);
		break;
	case 2:
		cpu_set_d(D & temp);
		break;
	case 3:
		cpu_set_e(E & temp);
		break;
	case 4:
		cpu_set_h(H & temp);
		break;
	case 5:
		cpu_set_l(L & temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) & temp);
		break;
	case 7:
		cpu_set_a(A & temp);
		break;
	}
	return 0;
};

uint8_t RES2()
{
	temp = ~(1 << 2);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B & temp);
		break;
	case 1:
		cpu_set_c(C & temp);
		break;
	case 2:
		cpu_set_d(D & temp);
		break;
	case 3:
		cpu_set_e(E & temp);
		break;
	case 4:
		cpu_set_h(H & temp);
		break;
	case 5:
		cpu_set_l(L & temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) & temp);
		break;
	case 7:
		cpu_set_a(A & temp);
		break;
	}
	return 0;
};

uint8_t RES3()
{
	temp = ~(1 << 3);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B & temp);
		break;
	case 1:
		cpu_set_c(C & temp);
		break;
	case 2:
		cpu_set_d(D & temp);
		break;
	case 3:
		cpu_set_e(E & temp);
		break;
	case 4:
		cpu_set_h(H & temp);
		break;
	case 5:
		cpu_set_l(L & temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) & temp);
		break;
	case 7:
		cpu_set_a(A & temp);
		break;
	}
	return 0;
};

uint8_t RES4()
{
	temp = ~(1 << 4);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B & temp);
		break;
	case 1:
		cpu_set_c(C & temp);
		break;
	case 2:
		cpu_set_d(D & temp);
		break;
	case 3:
		cpu_set_e(E & temp);
		break;
	case 4:
		cpu_set_h(H & temp);
		break;
	case 5:
		cpu_set_l(L & temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) & temp);
		break;
	case 7:
		cpu_set_a(A & temp);
		break;
	}
	return 0;
};

uint8_t RES5()
{
	temp = ~(1 << 5);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B & temp);
		break;
	case 1:
		cpu_set_c(C & temp);
		break;
	case 2:
		cpu_set_d(D & temp);
		break;
	case 3:
		cpu_set_e(E & temp);
		break;
	case 4:
		cpu_set_h(H & temp);
		break;
	case 5:
		cpu_set_l(L & temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) & temp);
		break;
	case 7:
		cpu_set_a(A & temp);
		break;
	}
	return 0;
};

uint8_t RES6()
{
	temp = ~(1 << 6);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B & temp);
		break;
	case 1:
		cpu_set_c(C & temp);
		break;
	case 2:
		cpu_set_d(D & temp);
		break;
	case 3:
		cpu_set_e(E & temp);
		break;
	case 4:
		cpu_set_h(H & temp);
		break;
	case 5:
		cpu_set_l(L & temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) & temp);
		break;
	case 7:
		cpu_set_a(A & temp);
		break;
	}
	return 0;
};

uint8_t RES7()
{
	temp = ~(1 << 7);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B & temp);
		break;
	case 1:
		cpu_set_c(C & temp);
		break;
	case 2:
		cpu_set_d(D & temp);
		break;
	case 3:
		cpu_set_e(E & temp);
		break;
	case 4:
		cpu_set_h(H & temp);
		break;
	case 5:
		cpu_set_l(L & temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) & temp);
		break;
	case 7:
		cpu_set_a(A & temp);
		break;
	}
	return 0;
};

// Rotate A left through carry
uint8_t RLA()
{
	temp = cpu_getflag(Cy) ? 1 : 0;
	cpu_setflag(Cy, (A & 0x80) > 0);
	cpu_set_a((A << 1) | temp);

	// Set flags
	cpu_setflag(Z, false);
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	return 0;
};

// Rotate left through carry
uint8_t RL()
{
	temp = cpu_getflag(Cy);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_setflag(Cy, (B & 0x80) > 0);
		cpu_set_b((B << 1) | temp);
		cpu_setflag(Z, B == 0);
		break;
	case 1:
		cpu_setflag(Cy, (C & 0x80) > 0);
		cpu_set_c((C << 1) | temp);
		cpu_setflag(Z, C == 0);
		break;
	case 2:
		cpu_setflag(Cy, (D & 0x80) > 0);
		cpu_set_d((D << 1) | temp);
		cpu_setflag(Z, D == 0);
		break;
	case 3:
		cpu_setflag(Cy, (E & 0x80) > 0);
		cpu_set_e((E << 1) | temp);
		cpu_setflag(Z, E == 0);
		break;
	case 4:
		cpu_setflag(Cy, (H & 0x80) > 0);
		cpu_set_h((H << 1) | temp);
		cpu_setflag(Z, H == 0);
		break;
	case 5:
		cpu_setflag(Cy, (L & 0x80) > 0);
		cpu_set_l((L << 1) | temp);
		cpu_setflag(Z, L == 0);
		break;
	case 6:
		cpu_setflag(Cy, (cpu_read(HL) & 0x80) > 0);
		cpu_write(HL, (cpu_read(HL) << 1) | temp);
		cpu_setflag(Z, cpu_read(HL) == 0);
		break;
	case 7:
		cpu_setflag(Cy, (A & 0x80) > 0);
		cpu_set_a((A << 1) | temp);
		cpu_setflag(Z, A == 0);
		break;
	}

	// Set flags
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	return 0;
};

// Rotate left
uint8_t RLC()
{
	switch (cb_opcode & 0x07)
	{
	case 0:
		temp = B >> 7;
		cpu_set_b((B << 1) | temp);
		cpu_setflag(Z, B == 0);
		break;
	case 1:
		temp = (C & 0x80) >> 7;
		cpu_set_c((C << 1) | temp);
		cpu_setflag(Z, C == 0);
		break;
	case 2:
		temp = (D & 0x80) >> 7;
		cpu_set_d((D << 1) | temp);
		cpu_setflag(Z, D == 0);
		break;
	case 3:
		temp = (E & 0x80) >> 7;
		cpu_set_e((E << 1) | temp);
		cpu_setflag(Z, E == 0);
		break;
	case 4:
		temp = (H & 0x80) >> 7;
		cpu_set_h((H << 1) | temp);
		cpu_setflag(Z, H == 0);
		break;
	case 5:
		temp = (L & 0x80) >> 7;
		cpu_set_l((L << 1) | temp);
		cpu_setflag(Z, L == 0);
		break;
	case 6:
		temp = (cpu_read(HL) & 0x80) >> 7;
		cpu_write(HL, (cpu_read(HL) << 1) | temp);
		cpu_setflag(Z, cpu_read(HL) == 0);
		break;
	case 7:
		temp = (A & 0x80) >> 7;
		cpu_set_a((A << 1) | temp);
		cpu_setflag(Z, A == 0);
		break;
	}

	// Set flags
	cpu_setflag(Cy, temp > 0);
	
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	return 0;
};

// Rotate A left
uint8_t RLCA()
{
	temp = (A & 0x80) >> 7;
	cpu_set_a((A << 1) | temp);

	// Set flags
	cpu_setflag(Cy, (bool)temp);
	cpu_setflag(Z, false);
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	return 0;
};

// Rotate right through carry
uint8_t RR()
{
	temp = cpu_getflag(Cy) << 7;
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_setflag(Cy, (B & 0x01) > 0);
		temp = (B >> 1) | temp;
		cpu_set_b(temp);
		break;
	case 1:
		cpu_setflag(Cy, (C & 0x01) > 0);
		temp = (C >> 1) | temp;
		cpu_set_c(temp);
		break;
	case 2:
		cpu_setflag(Cy, (D & 0x01) > 0);
		temp = (D >> 1) | temp;
		cpu_set_d(temp);
		break;
	case 3:
		cpu_setflag(Cy, (E & 0x01) > 0);
		temp = (E >> 1) | temp;
		cpu_set_e(temp);
		break;
	case 4:
		cpu_setflag(Cy, (H & 0x01) > 0);
		temp = (H >> 1) | temp;
		cpu_set_h(temp);
		break;
	case 5:
		cpu_setflag(Cy, (L & 0x01) > 0);
		temp = (L >> 1) | temp;
		cpu_set_l(temp);
		break;
	case 6:
		cpu_setflag(Cy, (cpu_read(HL) & 0x01) > 0);
		temp = (cpu_read(HL) >> 1) | temp;
		cpu_write(HL, temp);
		break;
	case 7:
		cpu_setflag(Cy, (A & 0x01) > 0);
		temp = (A >> 1) | temp;
		cpu_set_a(temp);
		break;
	}

	// Set flags
	cpu_setflag(Z, temp == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	return 0;
};

// Rotate A right through carry
uint8_t RRA()
{
	temp = cpu_getflag(Cy) << 7;
	cpu_setflag(Cy, (A & 0x01) > 0);
	cpu_set_a((A >> 1) | temp);

	// Set flags
	cpu_setflag(Z, false);
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	return 0;
};

// Rotate right
uint8_t RRC()
{
	switch (cb_opcode & 0x07)
	{
	case 0:
		temp = (B & 0x01) << 7;
		temp = (B >> 1) | temp;
		cpu_set_b(temp);
		break;
	case 1:
		temp = (C & 0x01) << 7;
		temp = (C >> 1) | temp;
		cpu_set_c(temp);
		break;
	case 2:
		temp = (D & 0x01) << 7;
		temp = (D >> 1) | temp;
		cpu_set_d(temp);
		break;
	case 3:
		temp = (E & 0x01) << 7;
		temp = (E >> 1) | temp;
		cpu_set_e(temp);
		break;
	case 4:
		temp = (H & 0x01) << 7;
		temp = (H >> 1) | temp;
		cpu_set_h(temp);
		break;
	case 5:
		temp = (L & 0x01) << 7;
		temp = (L >> 1) | temp;
		cpu_set_l(temp);
		break;
	case 6:
		temp = (cpu_read(HL) & 0x01) << 7;
		temp = (cpu_read(HL) >> 1) | temp;
		cpu_write(HL, temp);
		break;
	case 7:
		temp = (A & 0x01) << 7;
		temp = (A >> 1) | temp;
		cpu_set_a(temp);
		break;
	}

	// Set flags
	cpu_setflag(Cy, (temp & 0x80) > 0);
	cpu_setflag(Z, temp == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	return 0;
};

// Rotate A right
uint8_t RRCA()
{
	temp = (A & 0x01) << 7;
	cpu_set_a((A >> 1) | temp);

	// Set flags
	cpu_setflag(Cy, temp > 0);
	cpu_setflag(Z, false);
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	return 0;
};

// Reset to $00 - equivalent to CALL 00
uint8_t RST00()
{
	cpu_write(--SP, (uint8_t)(PC >> 8));
	cpu_write(--SP, (uint8_t)PC);
	PC = 0x0000;
	return 0;
};

uint8_t RST08()
{
	cpu_write(--SP, (uint8_t)(PC >> 8));
	cpu_write(--SP, (uint8_t)PC);
	PC = 0x0008;
	return 0;
};

uint8_t RST10()
{
	cpu_write(--SP, (uint8_t)(PC >> 8));
	cpu_write(--SP, (uint8_t)PC);
	PC = 0x0010;
	return 0;
};

uint8_t RST18()
{
	cpu_write(--SP, (uint8_t)(PC >> 8));
	cpu_write(--SP, (uint8_t)PC);
	PC = 0x0018;
	return 0;
};

uint8_t RST20()
{
	cpu_write(--SP, (uint8_t)(PC >> 8));
	cpu_write(--SP, (uint8_t)PC);
	PC = 0x0020;
	return 0;
};

uint8_t RST28()
{
	cpu_write(--SP, (uint8_t)(PC >> 8));
	cpu_write(--SP, (uint8_t)PC);
	PC = 0x0028;
	return 0;
};

uint8_t RST30()
{
	cpu_write(--SP, (uint8_t)(PC >> 8));
	cpu_write(--SP, (uint8_t)PC);
	PC = 0x0030;
	return 0;
};

uint8_t RST38()
{
	cpu_write(--SP, (uint8_t)(PC >> 8));
	cpu_write(--SP, (uint8_t)PC);
	PC = 0x0038;
	return 0;
};

// Subtract with carry
uint8_t SBC()
{
	//temp = fetched + cpu_getflag(Cy) ? 1 : 0;

	//// Set flags
	//cpu_setflag(Z, A == temp);
	//cpu_setflag(N, true);
	//cpu_setflag(Hc, (temp & 0x0F) > (A & 0x0F));
	//cpu_setflag(Cy, temp > A);

	//cpu_set_a(A - temp);
	/*cpu_setflag(Z, (uint8_t)(A + fetched + cpu_getflag(Cy)) == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, ((0x0F & A) + (0x0F & fetched) + cpu_getflag(Cy)) > 0x0F);
	cpu_setflag(Cy, ((0xFF & A) + (0xFF & fetched) + cpu_getflag(Cy)) > 0xFF);

	cpu_set_a(A - fetched - cpu_getflag(Cy));
	return 0;*/
	temp_16 = A - fetched - cpu_getflag(Cy);

	// Set flags
	cpu_setflag(Z, (uint8_t)temp_16 == 0);
	cpu_setflag(N, true);
	cpu_setflag(Hc, ((fetched & 0x0F) + cpu_getflag(Cy)) > (A & 0x0F));
	cpu_setflag(Cy, (uint16_t)(fetched + cpu_getflag(Cy)) > A);

	cpu_set_a(temp_16);
	return 0;
	
};

// Set carry flag
uint8_t SCF()
{
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	cpu_setflag(Cy, true);
	return 0;
};

// Set bit 0
uint8_t SET0()
{
	temp = (1 << 0);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B | temp);
		break;
	case 1:
		cpu_set_c(C | temp);
		break;
	case 2:
		cpu_set_d(D | temp);
		break;
	case 3:
		cpu_set_e(E | temp);
		break;
	case 4:
		cpu_set_h(H | temp);
		break;
	case 5:
		cpu_set_l(L | temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) | temp);
		break;
	case 7:
		cpu_set_a(A | temp);
		break;
	}
	return 0;
};

uint8_t SET1()
{
	temp = (1 << 1);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B | temp);
		break;
	case 1:
		cpu_set_c(C | temp);
		break;
	case 2:
		cpu_set_d(D | temp);
		break;
	case 3:
		cpu_set_e(E | temp);
		break;
	case 4:
		cpu_set_h(H | temp);
		break;
	case 5:
		cpu_set_l(L | temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) | temp);
		break;
	case 7:
		cpu_set_a(A | temp);
		break;
	}
	return 0;
};

uint8_t SET2()
{
	temp = (1 << 2);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B | temp);
		break;
	case 1:
		cpu_set_c(C | temp);
		break;
	case 2:
		cpu_set_d(D | temp);
		break;
	case 3:
		cpu_set_e(E | temp);
		break;
	case 4:
		cpu_set_h(H | temp);
		break;
	case 5:
		cpu_set_l(L | temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) | temp);
		break;
	case 7:
		cpu_set_a(A | temp);
		break;
	}
	return 0;
};

uint8_t SET3()
{
	temp = (1 << 3);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B | temp);
		break;
	case 1:
		cpu_set_c(C | temp);
		break;
	case 2:
		cpu_set_d(D | temp);
		break;
	case 3:
		cpu_set_e(E | temp);
		break;
	case 4:
		cpu_set_h(H | temp);
		break;
	case 5:
		cpu_set_l(L | temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) | temp);
		break;
	case 7:
		cpu_set_a(A | temp);
		break;
	}
	return 0;
};

uint8_t SET4()
{
	temp = (1 << 4);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B | temp);
		break;
	case 1:
		cpu_set_c(C | temp);
		break;
	case 2:
		cpu_set_d(D | temp);
		break;
	case 3:
		cpu_set_e(E | temp);
		break;
	case 4:
		cpu_set_h(H | temp);
		break;
	case 5:
		cpu_set_l(L | temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) | temp);
		break;
	case 7:
		cpu_set_a(A | temp);
		break;
	}
	return 0;
};

uint8_t SET5()
{
	temp = (1 << 5);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B | temp);
		break;
	case 1:
		cpu_set_c(C | temp);
		break;
	case 2:
		cpu_set_d(D | temp);
		break;
	case 3:
		cpu_set_e(E | temp);
		break;
	case 4:
		cpu_set_h(H | temp);
		break;
	case 5:
		cpu_set_l(L | temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) | temp);
		break;
	case 7:
		cpu_set_a(A | temp);
		break;
	}
	return 0;
};

uint8_t SET6()
{
	temp = (1 << 6);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B | temp);
		break;
	case 1:
		cpu_set_c(C | temp);
		break;
	case 2:
		cpu_set_d(D | temp);
		break;
	case 3:
		cpu_set_e(E | temp);
		break;
	case 4:
		cpu_set_h(H | temp);
		break;
	case 5:
		cpu_set_l(L | temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) | temp);
		break;
	case 7:
		cpu_set_a(A | temp);
		break;
	}
	return 0;
};

uint8_t SET7()
{
	temp = (1 << 7);
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_set_b(B | temp);
		break;
	case 1:
		cpu_set_c(C | temp);
		break;
	case 2:
		cpu_set_d(D | temp);
		break;
	case 3:
		cpu_set_e(E | temp);
		break;
	case 4:
		cpu_set_h(H | temp);
		break;
	case 5:
		cpu_set_l(L | temp);
		break;
	case 6:
		cpu_write(HL, cpu_read(HL) | temp);
		break;
	case 7:
		cpu_set_a(A | temp);
		break;
	}
	return 0;
};

// Shift left arithmetic
// C <- [7 <- 0] <- 0
uint8_t SLA()
{
	switch (cb_opcode & 0x07)
	{
	case 0:
		cpu_setflag(Cy, (B & 0x80) >> 7);
		temp = B << 1;
		cpu_set_b(temp);
		break;
	case 1:
		cpu_setflag(Cy, (C & 0x80) >> 7);
		temp = C << 1;
		cpu_set_c(temp);
		break;
	case 2:
		cpu_setflag(Cy, (D & 0x80) >> 7);
		temp = D << 1;
		cpu_set_d(temp);
		break;
	case 3:
		cpu_setflag(Cy, (E & 0x80) >> 7);
		temp = E << 1;
		cpu_set_e(temp);
		break;
	case 4:
		cpu_setflag(Cy, (H & 0x80) >> 7);
		temp = H << 1;
		cpu_set_h(temp);
		break;
	case 5:
		cpu_setflag(Cy, (L & 0x80) >> 7);
		temp = L << 1;
		cpu_set_l(temp);
		break;
	case 6:
		temp = cpu_read(HL);
		cpu_setflag(Cy, (temp & 0x80) >> 7);
		temp = temp << 1;
		cpu_write(HL, temp);
		break;
	case 7:
		cpu_setflag(Cy, (A & 0x80) >> 7);
		temp = A << 1;
		cpu_set_a(temp);
		break;
	}

	// Set flags
	cpu_setflag(Z, temp == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	return 0;
};

// Shift right arithmetic
// [7] -> [7 -> 0] -> C
uint8_t SRA()
{
	switch (cb_opcode & 0x07)
	{
	case 0:
		temp = B;
		cpu_set_b((temp & 0x80) | (temp >> 1));
		break;
	case 1:
		temp = C;
		cpu_set_c((temp & 0x80) | (temp >> 1));
		break;
	case 2:
		temp = D;
		cpu_set_d((temp & 0x80) | (temp >> 1));
		break;
	case 3:
		temp = E;
		cpu_set_e((temp & 0x80) | (temp >> 1));
		break;
	case 4:
		temp = H;
		cpu_set_h((temp & 0x80) | (temp >> 1));
		break;
	case 5:
		temp = L;
		cpu_set_l((temp & 0x80) | (temp >> 1));
		break;
	case 6:
		temp = cpu_read(HL);
		cpu_write(HL, (temp & 0x80) | (temp >> 1));
		break;
	case 7:
		temp = A;
		cpu_set_a((temp & 0x80) | (temp >> 1));
		break;
	}

	// Set flags
	cpu_setflag(Z, temp <= 1);
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	cpu_setflag(Cy, temp & 0x01);
	return 0;
};

// Shift right logic
// 0 -> [7 -> 0] -> C
uint8_t SRL()
{
	switch (cb_opcode & 0x07)
	{
	case 0:
		temp = B;
		cpu_set_b(temp >> 1);
		break;
	case 1:
		temp = C;
		cpu_set_c(temp >> 1);
		break;
	case 2:
		temp = D;
		cpu_set_d(temp >> 1);
		break;
	case 3:
		temp = E;
		cpu_set_e(temp >> 1);
		break;
	case 4:
		temp = H;
		cpu_set_h(temp >> 1);
		break;
	case 5:
		temp = L;
		cpu_set_l(temp >> 1);
		break;
	case 6:
		temp = cpu_read(HL);
		cpu_write(HL, temp >> 1);
		break;
	case 7:
		temp = A;
		cpu_set_a(temp >> 1);
		break;
	}

	// Set flags
	cpu_setflag(Z, temp <= 1);
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	cpu_setflag(Cy, temp & 0x01);
	return 0;
};

// Enter very low power mode
uint8_t STOP()
{
	cpu_halt = true;
	return 0;
};

uint8_t SUB()
{
	// Set flags
	cpu_setflag(Z, A == fetched);
	cpu_setflag(N, true);
	cpu_setflag(Hc, (fetched & 0x0F) > (A & 0x0F));
	cpu_setflag(Cy, fetched > A);

	cpu_set_a(A - fetched);
	return 0;
};

// Swap right nibble with left nibble
// where nibbles are 4 bits
uint8_t SWAP()
{
	switch (cb_opcode & 0x07)
	{
	case 0:
		temp = B;
		cpu_set_b((temp << 4) | (temp >> 4));
		break;
	case 1:
		temp = C;
		cpu_set_c((temp << 4) | (temp >> 4));
		break;
	case 2:
		temp = D;
		cpu_set_d((temp << 4) | (temp >> 4));
		break;
	case 3:
		temp = E;
		cpu_set_e((temp << 4) | (temp >> 4));
		break;
	case 4:
		temp = H;
		cpu_set_h((temp << 4) | (temp >> 4));
		break;
	case 5:
		temp = L;
		cpu_set_l((temp << 4) | (temp >> 4));
		break;
	case 6:
		temp = cpu_read(HL);
		cpu_write(HL, (temp << 4) | (temp >> 4));
		break;
	case 7:
		temp = A;
		cpu_set_a((temp << 4) | (temp >> 4));
		break;
	}

	// Set flags
	cpu_setflag(Z,temp == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	cpu_setflag(Cy, false);
	return 0;
};

uint8_t XOR()
{
	cpu_set_a(A ^ fetched);

	// Set flags
	cpu_setflag(Z, A == 0);
	cpu_setflag(N, false);
	cpu_setflag(Hc, false);
	cpu_setflag(Cy, false);
	return 0;
};

uint8_t XXX()
{
	return 0;
};

// 16-bit Instructions

// Add to HL
uint8_t ADDHL_16()
{	
	// Set flags
	cpu_setflag(N, false);
	cpu_setflag(Hc, ((HL & 0x0FFF) + (fetched_16 & 0x0FFF)) > 0x0FFF);
	cpu_setflag(Cy, ((int)HL + (int)fetched_16) > 0xFFFF);

	HL += fetched_16;
	return 0;
};

// Add signed 8-bit integer to SP
uint8_t ADDSP_16()
{
	// Set flags
	cpu_setflag(Hc, ((0x0F & (uint8_t)(SP)) + (0x0F & fetched)) > 0x0F);
	cpu_setflag(Cy, ((0xFF & (uint8_t)(SP)) + (0xFF & fetched)) > 0xFF);
	cpu_setflag(Z, false);
	cpu_setflag(N, false);
	

	SP += (int8_t)fetched;
	return 0;
};

// Decrement BC
uint8_t DECBC_16()
{
	BC--;
	return 0;
};

uint8_t DECDE_16()
{
	DE--;
	return 0;
};

uint8_t DECHL_16()
{
	HL--;
	return 0;
};

uint8_t DECSP_16()
{
	SP--;
	return 0;
};

// Increment BC
uint8_t INBC_16()
{
	BC++;
	return 0;
};

uint8_t INDE_16()
{
	DE++;
	return 0;
};

uint8_t INHL_16()
{
	HL++;
	return 0;
};

uint8_t INSP_16()
{
	SP++;
	return 0;
};

// Load BC
uint8_t LDBC_16()
{
	BC = fetched_16;
	return 0;
}

// Load DE
uint8_t LDDE_16()
{
	DE = fetched_16;
	return 0;
}

// Load HL
uint8_t LDHL_16()
{
	HL = fetched_16;
	return 0;
}

// Load HL SP + signed
uint8_t LDHLSP_16() 
{
	// Set flags
	cpu_setflag(Hc, ((uint16_t)(0x0F & (uint8_t)(SP)) + (0x0F & fetched)) > 0x0F);
	cpu_setflag(Cy, ((uint16_t)(0xFF & (uint8_t)(SP)) + (0xFF & fetched)) > 0xFF);
	cpu_setflag(Z, false);
	cpu_setflag(N, false);

	HL = SP + (int8_t)fetched;
}

// Load SP
uint8_t LDSP_16()
{
	SP = fetched_16;
	stack_base = SP;
	return 0;
}

// Write SP to (nn)
uint8_t LDASP_16()
{
	// Write one byte at a time
	cpu_write(fetched_16, (uint8_t) SP);
	cpu_write(fetched_16+1, ((uint8_t) (SP >> 8)));
	return 0;
}

uint8_t disassemble_inst(uint16_t inst_pointer, wchar_t* disassembled_inst, int strlen)
{
	wchar_t cb_op_regs[8][10] = {
		L"B", L"C", L"D", L"E", L"H", L"L", L"(HL)", L"A"
	};

	// Get opcode from pointer
	uint8_t dis_opcode = cpu_read(inst_pointer);
	uint8_t dis_fetched = 0x00;
	uint16_t dis_fetched_16 = 0x0000;
	uint8_t dis_cb_opcode = 0x00;

	// Get instruction length and string
	uint8_t inst_len = optable[dis_opcode].inst_len;
	wchar_t* dis_inst = optable[dis_opcode].repr;
	wchar_t dis_inst_temp[150] = { 0 };

	if (inst_len == 1)
		wcscpy_s(dis_inst_temp, 150, dis_inst);
	if (inst_len == 2) 
	{
		dis_fetched = cpu_read(inst_pointer + 1);

		// Handle CB prefix
		if (dis_opcode == 0xCB)
		{
			dis_inst = dis_precb[dis_fetched >> 3];
			StringCchPrintfW(dis_inst_temp, 150, dis_inst, cb_op_regs[dis_fetched & 0x7]);
		}
		else
		{
			StringCchPrintfW(dis_inst_temp, 150, dis_inst, dis_fetched);

			// Add useful comments for relative jumps
			wchar_t dis_comment[80] = { 0 };
			switch (dis_opcode) 
			{
				case 0x18:
				case 0x20:
				case 0x28:
				case 0x30:
				case 0x38:
				{
					StringCchPrintfW(dis_comment, 80, L"    ; Absolute address: $%04x", inst_pointer + (int8_t)dis_fetched + inst_len);
					wcscat_s(dis_inst_temp, 150, dis_comment);
					break;
				}
				case 0xE8:
				case 0xF8:
				{
					StringCchPrintfW(dis_comment, 80, L"    ; Signed number: %d", (int8_t)dis_fetched);
					wcscat_s(dis_inst_temp, 150, dis_comment);
					break;
				}
			}
		}
	}
	else if (inst_len == 3)
	{
		dis_fetched_16 = cpu_read(inst_pointer+2) << 8 | cpu_read(inst_pointer+1);
		StringCchPrintfW(dis_inst_temp, 150, dis_inst, dis_fetched_16);
	}

	// Copy string to output string
	wcscpy_s(disassembled_inst, strlen, dis_inst_temp);

	return inst_len;
}
