#ifndef GUI_CODE
#define GUI_CODE

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <strsafe.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <shellscalingapi.h>
#include <commctrl.h>
#include <math.h>


LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void UpdateLCD(HWND hwnd);
DWORD WINAPI game_play(HWND hwnd);
DWORD WINAPI game_step_frame(HWND hwnd);
void game_reset();
void UpdateInfo(HWND hwnd);
int RenderLCDScreen(HWND hwnd, void* screen_buffer, int nScale, int xOffset, int yOffset);
//void LCD_init();
//void LCD_free();
//void LCD_setPixel(int x, int y, COLOR2BIT color2bit);
//void LCD_on();
//void LCD_off();
void game_pause(HWND hwnd);

// Frame buffer pointer
extern uint32_t* frame_buffer;

struct cart_stats {
	int nTitle;
	char title[17];
	uint8_t type;
	uint8_t rom_size;
	uint8_t ram_size;
	uint8_t japan;
	bool header_chck;
	bool global_chck;
};

void displayCartStats();

#endif // #ifndef GUI_CODE