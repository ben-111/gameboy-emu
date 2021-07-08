#include "Bus.h"
#include "Joypad.h"

#define IF_ADDR 0xFF0F

// Static variables

// Joypad port: FF00
//static uint8_t JOYP = 0x00;

// Buttons
bool button_up		= false;
bool button_down	= false;
bool button_left	= false;
bool button_right	= false;
bool button_start	= false;
bool button_select	= false;
bool button_b		= false;
bool button_a		= false;

// Selects
bool select_buttons = false;
bool select_directions = false;

void joypad_reset()
{
	// All buttons depressed
	// Buttons
	bool button_up = false;
	bool button_down = false;
	bool button_left = false;
	bool button_right = false;
	bool button_start = false;
	bool button_select = false;
	bool button_b = false;
	bool button_a = false;

	// Selects
	bool select_buttons = false;
	bool select_directions = false;
}

uint8_t joypad_register_read(uint16_t addr)
{
	if (addr == 0xFF00)
	{
		uint8_t joyp = 0x00;
		if (select_buttons)
		{
			joyp |= JOYPAD_SELECT_BUTTONS;
			joyp |= button_start ? 0 : JOYPAD_BUTTON_DOWN_START;
			joyp |= button_select ? 0 : JOYPAD_BUTTON_UP_SELECT;
			joyp |= button_b ? 0 : JOYPAD_BUTTON_LEFT_B;
			joyp |= button_a ? 0 : JOYPAD_BUTTON_RIGHT_A;
		}
		if (select_directions)
		{
			joyp |= JOYPAD_SELECT_DIRECTIONS;
			joyp |= (button_down ? 0 : JOYPAD_BUTTON_DOWN_START);
			joyp |= (button_up ? 0 : JOYPAD_BUTTON_UP_SELECT);
			joyp |= (button_left ? 0 : JOYPAD_BUTTON_LEFT_B);
			joyp |= (button_right ? 0 : JOYPAD_BUTTON_RIGHT_A);
		}
		return joyp;
	}
	else
		return 0xFF;
}

uint8_t joypad_register_write(uint16_t addr, uint8_t data)
{
	if (addr == 0xFF00)
	{
		if (data & JOYPAD_SELECT_BUTTONS)
			select_buttons = true;
		else
			select_buttons = false;
		if (data & JOYPAD_SELECT_DIRECTIONS)
			select_directions = true;
		else
			select_directions = false;
	}
	return 0;
}

uint8_t joypad_read(uint16_t addr)
{
	return bus_read(addr, DEV_JOYPAD);
}

uint8_t joypad_write(uint16_t addr, uint8_t data)
{
	return bus_write(addr, data, DEV_JOYPAD);
}

uint8_t joypad_button_press(uint8_t button, bool bPress)
{
	bool req_int = false;
	switch (button)
	{
		case GB_JOYPAD_UP:
			if (select_directions && bPress && !button_up)
				req_int = true;
			button_up = bPress;
			break;
		case GB_JOYPAD_DOWN:
			if (select_directions && bPress && !button_down)
				req_int = true;
			button_down = bPress;
			break;
		case GB_JOYPAD_LEFT:
			if (select_directions && bPress && !button_left)
				req_int = true;
			button_left = bPress;
			break;
		case GB_JOYPAD_RIGHT:
			if (select_directions && bPress && !button_right)
				req_int = true;
			button_right = bPress;
			break;
		case GB_JOYPAD_START:
			if (select_buttons && bPress && !button_start)
				req_int = true;
			button_start = bPress;
			break;
		case GB_JOYPAD_SELECT:
			if (select_buttons && bPress && !button_select)
				req_int = true;
			button_select = bPress;
			break;
		case GB_JOYPAD_B:
			if (select_buttons && bPress && !button_b)
				req_int = true;
			button_b = bPress;
			break;
		case GB_JOYPAD_A:
			if (select_buttons && bPress && !button_a)
				req_int = true;
			button_a = bPress;
			break;
	}
	/*if (select_buttons && (button_start || button_select || button_b || button_a)) // No, on change
	{
		req_int = true;
	}
	if (select_directions && (button_up || button_down || button_left || button_right))
	{
		req_int = true;
	}
	*/
	if (req_int)
	{
		// Request interrupt if any of the selected buttons is pressed
		joypad_write(IF_ADDR, joypad_read(IF_ADDR) | INT_JOYPAD);
	}
	return 0;
}
