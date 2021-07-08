#ifndef TIMER_CODE
#define TIMER_CODE

void timer_reset();
void timer_clock();
uint8_t timer_register_read(uint16_t addr);
uint8_t timer_register_write(uint16_t addr, uint8_t data);
uint8_t timer_read(uint16_t addr);
uint8_t timer_write(uint16_t addr, uint8_t data);

enum TAC_FLAGS{
	TAC_FREQ_SELECT_LO = (1 << 0),
	TAC_FREQ_SELECT_HI = (1 << 1),
	TAC_ENABLE = (1 << 2)
};

enum TAC_FREQS {
	TAC_FREQ_1024 = 0,
	TAC_FREQ_16,
	TAC_FREQ_64,
	TAC_FREQ_256
};


#endif // #ifndef TIMER_CODE