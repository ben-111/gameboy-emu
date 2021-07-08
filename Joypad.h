#ifndef JOYPAD_CODE
#define JOYPAD_CODE

void joypad_reset();
uint8_t joypad_register_read(uint16_t addr);
uint8_t joypad_register_write(uint16_t addr, uint8_t data);
uint8_t joypad_read(uint16_t addr);
uint8_t joypad_write(uint16_t addr, uint8_t data);
uint8_t joypad_button_press(uint8_t button, bool bPress);

enum JOYPAD_FLAGS {
	JOYPAD_SELECT_BUTTONS		= (1 << 4),
	JOYPAD_SELECT_DIRECTIONS	= (1 << 5),
	JOYPAD_BUTTON_DOWN_START	= (1 << 3),
	JOYPAD_BUTTON_UP_SELECT		= (1 << 2),
	JOYPAD_BUTTON_LEFT_B		= (1 << 1),
	JOYPAD_BUTTON_RIGHT_A		= (1 << 0)
};

#endif // #ifndef JOYPAD_CODE