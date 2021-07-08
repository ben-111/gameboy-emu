#include "Bus.h"
#include "Ppu.h"
#include "Emulator_GUI.h"
#include "Dma.h"

// Initialize static variables
// LCD control: FF40
static uint8_t LCDC = 0x00;

// LCD status: FF41
static uint8_t STAT = 0x00;

// Vertica scroll: FF42
static uint8_t SCY = 0x00;

// Horizontal scroll: FF43
static uint8_t SCX = 0x00;

// Scanline: FF44
static uint8_t LY = 0x00;

// Scanline compare: FF45
static uint8_t LYC = 0x00;

// Background & window palette: FF47
static uint8_t BGP = 0x00;

// Object palette 0 & 1: FF48, FF49
static uint8_t OBP0 = 0x00;
static uint8_t OBP1 = 0x00;

// Window X & Y position: FF4A, FF4B
static uint8_t WY = 0x00;
static uint8_t WX = 0x00;

// Register addresses:
// LCDC	0xFF40
// STAT	0xFF41
// LY	0xFF44
// LY	0xFF45
// IF	0xFF0F
// SCY	0xFF42
// SCX	0xFF43
// BGP	0xFF47
// OBP0	0xFF48
// OBP1	0xFF49
//
// VRAM and OAM
// Video RAM: 8000 - 9FFF
// The VRAM is divided to ~4 parts:
// 8000 - 8FFF: Sprite Tiles
// 8000 - 8FFF / 8800 - 97FF: BG Tiles	 [LCDC_BG_TILE_DATA]
// 9800 - 9BFF / 9C00 - 9FFF: BG Map	 [LCDC_BG_TILE_MAP]
// 9800 - 9BFF / 9C00 - 9FFF: Window Map [LCDC_WINDOW_TILE_MAP]
//
// Screen:
// The LCD screen is 160 pixels across and 144 lines long.
//
// Tiles:
// Each tile is 8x8 2 bit color pixels. Every row is 16 bits, where the first 8 bits are the
// upper part of the color number, and the last 8 bits are the lower part of the color number.
// For example, let's say the colors numbers 0-3 are black [b], dgray [d], lgray [l], white [w].
// So this row: 00110101 11010011 is gonna be: ddlwbldw.
// Over all, every pixel is 2 bytes * 8 rows = 16 bytes, and there are a total of 256 tiles that can be 
// saved for BG / sprite tiles (Total, however, is 384 tiles).
// On the screen, there are 20 * 18 = 360 tiles displayed, which of course can't all be unique.
// However, the VRAM tile map is 32 * 32 = 1024 tiles, and what we see on screen is a viewport into the full 
// map which can be moved around via the scroll registers.
//
// Maps:
// The map is 32 * 32 = 1024 bytes, each byte representing a tile. Each byte is an index into the tile data, however
// the index number depends on the mode [LCDC_BG_TILE_DATA]. In mode 1, the index is an unsigned number, meaning from 0 - 255,
// from addresses $8000 - $8FFF.
// In mode 0, the index is a signed number, meaning from -127 - 128, where the base pointer is at address $9000,
// so the tiles can be selected from addresses $8800 - $97FF.
// There are 2 tile maps in memory, one is from $9800-$9BFF, the other is from $9C00-$9FFF. The BG and the Window
// can use either of them.
static uint8_t vram[8192];

// OAM - Object Attribute Memory (Object == Sprite)
// $FE00 - $FE9F
// The OAM is 160 bytes, which can hold 40 sprites of 4 bytes each.
//
// Sprites:
// Each sprite consists of 4 bytes:
// Byte 0: Y position (minus 16, since sprites are optionally 8x16 and need to be scrolled in)
// Byte 1: X position (minus 8, since sprites need to be scrolled in)
// Byte 2: Tile index (This is always an index from address $8000)
// Byte 3: Sprite attributes
//		Bit 7: OBJ-to-BG priority (0=OBJ above BG, 1=OBJ below BG except where BG color is 0)
//		Bit 6: Y-flip (0=normal)
//		Bit 5: X-flip (0=normal)
//		Bit 4: Palette number (0=OBP0, 1=OBP1)
//		Bits 3-0: Unused on DMG
static uint8_t oam[160];

// Dot counter for each line
static uint16_t line_dots = 0x0000;


// Sprite reference
// This arrray holds 10 X positions of sprites and their index in the OAM.
static OBJ_REF sprite_ref[10] = { 0 };

// Interrupt requests
static uint8_t int_req = 0x00;
bool int_check = false;

// The PPU is a finite state machine
// It has 4 modes:
// 1. 00 - Horizontal blank period
// 2. 01 - Vertical blank period
// 3. 10 - OAM search
// 4. 11 - Data transfer, or drawing
//
// Every new frame the cycle restarts. The pattern as so:
// For 144 lines:
//     1. OAM search - 80 dots
//     2. Data transfer - variable, 168 to 291, depending on amount of sprites on that line
//     3. Horizontal blank period - variable, 85 to 208, makes it so an entire line takes 456 dots
// For 10 lines:
//     1. Vertical blank period - 456 dots, effectively 4560 dots
//
// Each mode affects certain registers:
// 1. STAT register
// +-----+-------------------------------------------------------------------------------------------+
// | Bit | Description                                                                               |
// +-----+-------------------------------------------------------------------------------------------+
// |  2  | Coincidence Flag - When the value in the LY register (which is ReadOnly) is equal to the  |
// |     | value in LYC register (R/W) this flags sets.                                              |
// +-----+-------------------------------------------------------------------------------------------+
// | 0-1 | Mode Flag - These two bits show the current mode of the PPU, where 00 - Horizontal blank, |
// |     | 01 - Vertical blank, 10 - OAM search, 11 - Data transfer.                                 |
// +-----+-------------------------------------------------------------------------------------------+
// 2. LY register - Contains the current line that the PPU is on. Ranges from 0 to 153, where 144 through 153
// indicate the V-Blank period.
//
// Additionaly, each mode affects the memory
// that is accessible by the CPU.
// 1. In the OAM search, the OAM is inaccessible to the CPU. The VRAM is accessible.
// 2. In the Data transfer, the OAM and the VRAM are inaccessible to the CPU.
// 3. In Horizontal and Vertical blank periods, VRAM and OAM are accessible to the CPU.
//
// There are two types of interrupts that the PPU can trigger:
// 1. V-Blank Interrupt (INT 40) - If enabled (In the STAT register) it occurs every time the PPU gets
// to the V-Blank period.
// 2. LCDC Status interrupt (INT 48) - There are several interrupts that can be enabled from the STAT register,
// they are:
//     1. Coincidence interrupt - if enabled, will occur when LY == LYC.
//     2. OAM interrupt - if enabled, will occur when the PPU enters OAM search mode.
//     3. V-Blank interrupt - if enabled, will trigger V-Blank Interrupt when the PPU enters V-Blank mode.
//     4. H-Blank interrupt - if enabled, will occur when the PPU enters H-Blank mode.
// The LCDC Status Interrupt is triggered when transitioning from "No conditions met" to "Any conditions met".
// This means that if one condition is met right after a different one, it will not fire the interrupt because
// it transitioned from "Some conditions met" to "Some conditions met".
// 
// There are many settings that can be controlled from the LCD Control register.
// +-----+----------------------------------------------------------------------------------------------+
// | Bit | Description																					|
// +-----+----------------------------------------------------------------------------------------------+
// |  7  | LCD Display enable - If set, turns the LCD and the PPU on.									|
// +-----+----------------------------------------------------------------------------------------------+
// |  6  | Window tile map display select - When not set, the window uses the $9800 tile map. When set,	|
// |     | it uses the $9C00 one.																		|
// +-----+----------------------------------------------------------------------------------------------+
// |  5  | Window display enable - When set, the window will be rendered.								|
// +-----+----------------------------------------------------------------------------------------------+
// |  4  | BG and Window tile data select - If set, will use $8000 addressing mode. Otherwise, will use	|
// |     | $8800 addressing mode.																		|
// +-----+----------------------------------------------------------------------------------------------+
// |  3  | BG tile map display select - When not set, the BG uses the $9800 tile map. When set, it uses	|
// |     | the $9C00 one.																				|
// +-----+----------------------------------------------------------------------------------------------+
// |  2  | OBJ size - Sprite size select. When not set, the sprites are 8x8 pixels. When set, they are	|
// |     | 8x16 pixels.																					|
// +-----+----------------------------------------------------------------------------------------------+
// |  1  | OBJ display - When set, sprites are rendered.												|
// +-----+----------------------------------------------------------------------------------------------+
// |  0  | BG display - When set, the BG and Window (if enabled) are rendered. Otherwise, they are both	|
// |     | blank (even if bit 5 is set). Sprites can be enabled when the BG is not.						|
// +-----+----------------------------------------------------------------------------------------------+
// It's worthy to note that the LCDC register can be altered mid-scanline.
void ppu_clock()
{
	if (lcdc_getflag(LCDC_LCD_ENABLE))
	{
		switch (stat_getmode()) {
			case STAT_MODE_HBLANK:		// Horizontal blank (H-BLANK)
				hblank();
				break;
			case STAT_MODE_VBLANK:		// Vertical blank (V-BLANK)
				vblank();
				break;
			case STAT_MODE_OAM:		// OAM search
				// This mode takes exactly 80 dots, and here is what happens:
				// Every 2 dots the PPU checks an OAM entry to see if it's Y position happens
				// to be also on the current line (Sprite Y <= Current line <= Sprite Y + 8/16).
				// There are 10 Sprite comparitors that hold that information and check whether
				// there should be a sprite drawn, and that is why only 10 sprites can be on a line
				// at a time.
				oam_search();
				break;
			case STAT_MODE_DATA:		// Read OAM and VRAM to generate picture 
				data_transfer();
				break;
		}

		// Coincidence
		if (LYC == LY)
		{
			stat_setflag(STAT_COINCIDENCE, true);
			int_req_set(STAT_INT_REQ_COIN, true);
		}
		else
		{
			stat_setflag(STAT_COINCIDENCE, false);
			int_req_set(STAT_INT_REQ_COIN, false);
		}

		// Handle interrupts
		if (int_check)
		{
			int_check = false;
			bool lcdc_int = false;
			//bool vblank_int = false;
			if (int_req & STAT_INT_REQ_COIN && STAT & STAT_COINCIDENCE_INT)
			{
				// Reset request
				lcdc_int = true;
			}
			if (int_req & STAT_INT_REQ_OAM && STAT & STAT_OAM_INT)
			{
				// Reset request
				lcdc_int = true;
			}
			if (int_req & STAT_INT_REQ_VBLANK && STAT & STAT_VBLANK_INT)
			{
				// Reset request
				lcdc_int = true;
			}
			if (int_req & STAT_INT_REQ_HBLANK && STAT & STAT_HBLANK_INT)
			{
				// Reset request
				lcdc_int = true;
			}

			if (lcdc_int)
			{
				cpu_int_req_set(INT_LCDSTAT, true);
			}
			/*if (vblank_int)
			{
				cpu_int_req_set(INT_VBLANK, true);
			}*/
		}

		line_dots++;
		if (line_dots > 455)
		{
			line_dots = 0;

			// Update LY
			LY++;
			if (LY > 153)
				LY = 0;
		}
	}
}

#define sprite_top(y) ((y < 16) ? 0 : y - 16)
#define sprite_bottom(y) (lcdc_getflag(LCDC_OBJ_SIZE) ? y-1 : y-9)
void oam_search()
{
	// Static variables
	static uint8_t sprite_ref_index = 0;

	// At beginning of OAM search
	if (line_dots == 0)
	{
		// Reset the sprite reference memory
		for (int i = 0; i < 10; i++)
		{
			sprite_ref[i] = (OBJ_REF){0, 0};
		}

		// Request interrupt
		int_req_set(STAT_INT_REQ_OAM, true);
	}

	// Every two dots
	if (line_dots % 2 == 0)
	{
		uint8_t sprite_y = oam[2 * line_dots]; // Y position of sprite minus 16
		if ((sprite_ref_index < 10) && (LY + 16 >= sprite_y) && (LY + 16 < sprite_y + 8 + 8 * lcdc_getflag(LCDC_OBJ_SIZE)))
		{
			sprite_ref[sprite_ref_index++] = (OBJ_REF){ oam[2 * line_dots  + 1], line_dots / 2 }; // X position, index
		}
	}

	// Reset and set next mode
	if (line_dots == 79)
	{
		sprite_ref_index = 0;
		stat_setmode(STAT_MODE_DATA);

		// reset interrupt
		int_req_set(STAT_INT_REQ_OAM, false);
	}
}

void data_transfer()
{
	// Static variables
	static uint8_t draw_x = 0x00;
	static uint8_t pixel_fetcher_state = 0x00; // PF_STATES
	static uint8_t pixel_fetcher_mode = 0x00;
	static uint8_t pixel_fetcher_obj_state = 0x00; // Sprite state
	static uint16_t pixel_fetcher_tile_map_offset = 0x0000;
	static uint16_t pixel_fetcher_tile_offset = 0x0000;
	static uint16_t pixel_fetcher_obj_tile_offset = 0x0000;
	static uint8_t pixel_fetcher_x = 0x00;
	static uint8_t pixel_fetcher_tile_lo = 0x00;
	static uint8_t pixel_fetcher_tile_hi = 0x00;
	static bool push_stop = false; // True - stop pushing pixels to screen
	static uint8_t scx_counter = 0x00;
	static uint8_t curr_sprite_index = 0;

	// Pixel FIFOs
	// The pixel FIFO can hold 16 pixels. It must have at least 8 pixels in it before it can output
	// a pixel. That is so mixing between the BG and the sprites could be done. Mixing will be done on the high fifo.
	// Every pixel is composed of the color number (2 bits) and the palette (2 bits).
	// It is a queue.
	static uint32_t out_pixel_fifo = 0x00000000;
#define out_pixel_fifo_hi (uint16_t)(out_pixel_fifo >> 8)
#define out_pixel_fifo_lo (uint16_t)out_pixel_fifo
	static uint32_t out_palette_fifo = 0x00000000;
#define out_palette_fifo_hi (uint16_t)(out_palette_fifo >> 8)
#define out_palette_fifo_lo (uint16_t)out_palette_fifo
	static uint8_t obj_pixels_lo = 0x00;
	static uint8_t obj_pixels_hi = 0x00;
	static uint16_t obj_pixels = 0x0000;
	static uint8_t counter = 0;
	static uint8_t none_counter = 0;

	// Reset on the first cycle
	if (line_dots == 80)
	{
		out_pixel_fifo = 0x00000000;
		out_palette_fifo = 0x00000000;
		obj_pixels = 0x0000;
		pixel_fetcher_x = 0;
		draw_x = 0;
		scx_counter = SCX % 8;
		none_counter = 0;
		counter = 0;
		pixel_fetcher_state = PF_STATE_GET_TILE;
		pixel_fetcher_mode = PF_MODE_BG;
		push_stop = true;
	}

	// Pixel fetcher
	// The pixel fetcher gets a tile map address and has 4 states of operation:
	// 1. Get tile - gets the tile it needs to draw based on current X, Y, SCX, and SCY. Takes two cycles.
	// 2. Get tile data low - gets the lower 8 bits of the pixels needed from the tile. Takes two cycles.
	// 3. Get tile data high - gets the higher 8 bits of the pixels needed from the tile. Takes two cycles.
	// 4. Push - attempt to push the 8 pixels to the low part of the fifo. You can only push if it's empty.
	// The pixel fetcher also has a sprite mode and a window mode.
	
	static uint8_t obj_counter = 0;
	static bool push_pixels = false;

	if (!lcdc_getflag(LCDC_BG_ENABLE))
	{
		pixel_fetcher_mode = PF_MODE_NONE;
	}

	// Check for window and reset
	if (lcdc_getflag(LCDC_BG_ENABLE) && lcdc_getflag(LCDC_WINDOW_ENABLE) && (draw_x == (WX < 7 ? 0 : (WX - 7))) && (LY >= WY) && pixel_fetcher_mode == PF_MODE_BG)
	{
		// Clear fifos and set mode to window
		out_pixel_fifo = 0x00000000;
		out_palette_fifo = 0x00000000;

		pixel_fetcher_x = 0;
		pixel_fetcher_state = PF_STATE_GET_TILE;
		pixel_fetcher_mode = PF_MODE_WINDOW;
		counter = 0;
		push_stop = true;
	}

	// Check for window mode
	if (lcdc_getflag(LCDC_BG_ENABLE) && lcdc_getflag(LCDC_WINDOW_ENABLE) && (draw_x >= (WX - 7)) && (LY >= WY) && pixel_fetcher_mode == PF_MODE_BG)
	{
		pixel_fetcher_mode = PF_MODE_WINDOW;
	}

	// Push pixels to screen
	// Should only push if there are more than 8 pixels in the fifo
	if (!push_stop)
	{
		static uint8_t sprite_x_index = 0;

		// Check for sprites
		if (lcdc_getflag(LCDC_OBJ_ENABLE) && scx_counter == 0)// && false)
		{

			for (int i = sprite_x_index; i < 10; i++)
			{
				if ((sprite_ref[i].x_pos - 8 == draw_x) || ((draw_x == 0) && (sprite_ref[i].x_pos < 8) && (sprite_ref[i].x_pos > 0)))
				{
					sprite_x_index = i + 1;
					curr_sprite_index = sprite_ref[i].index;
					pixel_fetcher_obj_state = PF_STATE_GET_TILE;
					pixel_fetcher_mode = PF_MODE_OBJ;
					obj_counter = 0;
					push_stop = true;

					break;
				}
			}

		}
		COLOR2BIT pixel_color_applied = 0x00;
		if (!push_stop)
		{
			uint8_t pixel_color = 0x00;
			uint8_t pixel_palette = 0x00;
			

			// Get pixel info
			pixel_color = out_pixel_fifo >> 30;
			pixel_palette = out_palette_fifo >> 30;

			// Shift fifos
			out_pixel_fifo <<= 2;
			out_palette_fifo <<= 2;

			// Apply color palette
			if (pixel_palette == PALETTE_OBP0)
				pixel_color_applied = ((COLOR2BIT)(OBP0 >> 2 * pixel_color) & 0x03);
			else if (pixel_palette == PALETTE_OBP1)
				pixel_color_applied = ((COLOR2BIT)(OBP1 >> 2 * pixel_color) & 0x03);
			else if (pixel_palette == PALETTE_BGP)
				pixel_color_applied = ((COLOR2BIT)(BGP >> 2 * pixel_color) & 0x03);
		}
		// Set pixel
		if (scx_counter > 0 && pixel_fetcher_mode == PF_MODE_BG)
		{
			scx_counter--;
		}
		else
		{
			
			if (!push_stop)
			{
				sprite_x_index = 0;
				gameboy_screen_set_pixel(draw_x, LY, pixel_color_applied);
				draw_x++;
			}

		}

	}

	// Fetch pixels
	switch (pixel_fetcher_mode)
	{
		case PF_MODE_WINDOW:
		{
			// The window is a static area that renders over the BG. It starts from WX-7 and WY.
			switch (pixel_fetcher_state)
			{
			case PF_STATE_GET_TILE:
			{
				if (counter == 0)
				{
					pixel_fetcher_tile_map_offset = lcdc_getflag(LCDC_WINDOW_TILE_MAP) ? 0x1c00 : 0x1800;
					uint8_t tile_x = pixel_fetcher_x / 8;
					uint8_t tile_y = (LY - WY) / 8;
					uint8_t tile_index = vram[pixel_fetcher_tile_map_offset + tile_x + 32 * tile_y];
					pixel_fetcher_tile_offset = lcdc_getflag(LCDC_BG_TILE_DATA) ? (tile_index * TILE_SIZE) : (0x1000 + (int8_t)(tile_index)*TILE_SIZE);
				}
				else
					pixel_fetcher_state = PF_STATE_GET_TILE_DATA_LO;
				break;
			}
			case PF_STATE_GET_TILE_DATA_LO:
			{
				if (counter == 0)
				{
					pixel_fetcher_tile_lo = vram[pixel_fetcher_tile_offset + 2 * ((LY - WY) % 8)];
				}
				else
					pixel_fetcher_state = PF_STATE_GET_TILE_DATA_HI;
				break;
			}
			case PF_STATE_GET_TILE_DATA_HI:
			{
				if (counter == 0)
				{

					pixel_fetcher_tile_hi = vram[pixel_fetcher_tile_offset + 2 * ((LY - WY) % 8) + 1];
				}
				else if (pixel_fetcher_x == 0)
					pixel_fetcher_state = PF_STATE_PUSH;
				else
					pixel_fetcher_state = PF_STATE_SLEEP;
				break;
			}
			case PF_STATE_SLEEP:
			{
				if (counter == 1)
				{
					// Push
					//push_pixels = true;
					uint16_t temp_fifo = 0x0000;
					for (int i = 0; i < 8; i++)
					{
						temp_fifo <<= 2;
						temp_fifo |= (pixel_fetcher_tile_lo >> 7) | ((pixel_fetcher_tile_hi >> 7) << 1);

						pixel_fetcher_tile_lo <<= 1;
						pixel_fetcher_tile_hi <<= 1;

					}
					out_pixel_fifo |= temp_fifo;
					if (push_stop)
						push_stop = false;
					pixel_fetcher_x += 8;
					pixel_fetcher_state = PF_STATE_GET_TILE;
				}
				break;
			}
			case PF_STATE_PUSH:
			{
				uint16_t temp_fifo = 0x0000;
				for (int i = 0; i < 8; i++)
				{
					temp_fifo <<= 2;
					temp_fifo |= (pixel_fetcher_tile_lo >> 7) | ((pixel_fetcher_tile_hi >> 7) << 1);
					
					pixel_fetcher_tile_lo <<= 1;
					pixel_fetcher_tile_hi <<= 1;
				}
				out_pixel_fifo |= temp_fifo << 16;
				pixel_fetcher_x += 8;
				counter = 1;
				pixel_fetcher_state = PF_STATE_GET_TILE;
				break;
			}
			}
			counter++;
			if (counter > 1)
				counter = 0;
			break;
		}
		case PF_MODE_OBJ:
		{
			switch (pixel_fetcher_obj_state)
			{
				case PF_STATE_GET_TILE:
				{
					if (obj_counter == 0)
					{
						pixel_fetcher_obj_tile_offset = oam[curr_sprite_index * 4 + 2] & (lcdc_getflag(LCDC_OBJ_SIZE) ? 0xFE : 0xFF);
					}
					else
						pixel_fetcher_obj_state = PF_STATE_GET_TILE_DATA_LO;
					break;
				}
				case PF_STATE_GET_TILE_DATA_LO:
				{
					if (obj_counter == 0)
					{
						uint8_t sprite_y = oam[curr_sprite_index * 4];
						uint8_t sprite_attributes = oam[curr_sprite_index * 4 + 3];
						uint8_t sprite_height = lcdc_getflag(LCDC_OBJ_SIZE) ? 16 : 8;
						bool sprite_yflip = sprite_attributes & OBJ_ATTR_Y_FLIP;
						obj_pixels_lo = vram[pixel_fetcher_obj_tile_offset * 16 + 2 * (sprite_yflip ? (sprite_height - 1 - (LY - sprite_y + 16)) : (LY - sprite_y + 16))];
					}
					else
						pixel_fetcher_obj_state = PF_STATE_GET_TILE_DATA_HI;
					break;
				}
				case PF_STATE_GET_TILE_DATA_HI:
				{
					if (obj_counter == 0)
					{
						uint8_t sprite_y = oam[curr_sprite_index * 4];
						uint8_t sprite_attributes = oam[curr_sprite_index * 4 + 3];
						uint8_t sprite_height = lcdc_getflag(LCDC_OBJ_SIZE) ? 16 : 8;
						bool sprite_yflip = sprite_attributes & OBJ_ATTR_Y_FLIP;
						obj_pixels_hi = vram[pixel_fetcher_obj_tile_offset * 16 + 2 * (sprite_yflip ? (sprite_height - 1 - (LY - sprite_y + 16)) : (LY - sprite_y + 16)) + 1];
					}
					else
					{
						// Mix pixels
						
						uint8_t sprite_attributes = oam[curr_sprite_index * 4 + 3];
						bool bg_priority = sprite_attributes & OBJ_ATTR_BG_PRIORITY;
						bool x_flip = sprite_attributes & OBJ_ATTR_X_FLIP;
						for (int i = 0; i < 8; i++)
						{
							if (!x_flip)
							{
								obj_pixels <<= 2;
								obj_pixels |= ((obj_pixels_hi >> 7) << 1) | (obj_pixels_lo >> 7);

								obj_pixels_lo <<= 1;
								obj_pixels_hi <<= 1;
							}
							else
							{
								obj_pixels <<= 2;
								obj_pixels |= ((obj_pixels_hi << 1) & 0x02) | (obj_pixels_lo & 0x01);

								obj_pixels_lo >>= 1;
								obj_pixels_hi >>= 1;
							}
						}
						uint8_t sprite_x = oam[curr_sprite_index * 4 + 1];
						if (draw_x == 0 && sprite_x < 8)
						{
							obj_pixels <<= 2 * (8 - sprite_x);
						}
						uint16_t temp_fifo = 0x00;
						uint16_t temp_pfifo = 0x00;
						if (!bg_priority)
						{
							for (int i = 7; i >= 0; i--)
							{
								temp_fifo <<= 2;
								temp_pfifo <<= 2;
								temp_fifo |= ((((obj_pixels >> 2*i) & 0x03) > 0) && ((out_palette_fifo >> (16 + 2*i)) & 0x03) == PALETTE_BGP) ? ((obj_pixels >> 2*i) & 0x03) : ((out_pixel_fifo >> (16 + 2*i)) & 0x03);
								temp_pfifo |= ((((obj_pixels >> 2 * i) & 0x03) > 0) && ((out_palette_fifo >> (16 + 2 * i)) & 0x03) == PALETTE_BGP) ? ((sprite_attributes & OBJ_ATTR_PALETTE) ? PALETTE_OBP1 : PALETTE_OBP0) : ((out_palette_fifo >> (16 + 2*i)) & 0x03);
							}
						}
						else
						{
							for (int i = 7; i >= 0; i--)
							{
								temp_fifo <<= 2;
								temp_pfifo <<= 2;
								temp_fifo |= ((((obj_pixels >> 2 * i) & 0x03) > 0) && (((out_pixel_fifo >> (16 + 2*i)) & 0x03) == 0 && ((out_palette_fifo >> (16 + 2 * i)) & 0x03) == PALETTE_BGP) ? ((obj_pixels >> 2 * i) & 0x03) : ((out_pixel_fifo >> (16 + 2 * i))) & 0x03);
								temp_pfifo |= (((obj_pixels >> 2 * i) & 0x03) > 0) && (((out_pixel_fifo >> (16 + 2 * i)) & 0x03) == 0 && ((out_palette_fifo >> (16 + 2 * i)) & 0x03) == PALETTE_BGP) ? ((sprite_attributes & OBJ_ATTR_PALETTE) ? PALETTE_OBP1 : PALETTE_OBP0) : ((out_palette_fifo >> (16 + 2 * i)) & 0x03);
							}
						}
						out_pixel_fifo = (temp_fifo << 16) | (uint16_t)out_pixel_fifo;
						out_palette_fifo = (temp_pfifo << 16) | (uint16_t)out_palette_fifo;

						// Bring back BG mode
						pixel_fetcher_mode = PF_MODE_BG;
						push_stop = false;
					}
					
					break;
				}
			}
			obj_counter++;
			if (obj_counter > 1)
				obj_counter = 0;
			break;
		}
		case PF_MODE_BG:
		{
			switch (pixel_fetcher_state) 
			{
				case PF_STATE_GET_TILE:
				{
					if (counter == 0)
					{
						pixel_fetcher_tile_map_offset = lcdc_getflag(LCDC_BG_TILE_MAP) ? 0x1c00 : 0x1800;
						uint8_t tile_x = (uint8_t)(pixel_fetcher_x + SCX) / 8;
						uint8_t tile_y = (uint8_t)(LY + SCY) / 8;
						uint16_t tile_index = vram[pixel_fetcher_tile_map_offset + tile_x + 32 * tile_y];
						pixel_fetcher_tile_offset = lcdc_getflag(LCDC_BG_TILE_DATA) ? (tile_index * TILE_SIZE) : (0x1000 + (int8_t)(tile_index)*TILE_SIZE);
					}
					else
						pixel_fetcher_state = PF_STATE_GET_TILE_DATA_LO;
					break;
				}
				case PF_STATE_GET_TILE_DATA_LO:
				{
					if (counter == 0)
					{
						pixel_fetcher_tile_lo = vram[pixel_fetcher_tile_offset + 2 * ((LY + SCY) % 8)];
					}
					else
						pixel_fetcher_state = PF_STATE_GET_TILE_DATA_HI;
					break;
				}
				case PF_STATE_GET_TILE_DATA_HI:
				{
					if (counter == 0)
					{

						pixel_fetcher_tile_hi = vram[pixel_fetcher_tile_offset + 2 * ((LY + SCY) % 8) + 1];
					}
					else if (pixel_fetcher_x == 0)
						pixel_fetcher_state = PF_STATE_PUSH;
					else
						pixel_fetcher_state = PF_STATE_SLEEP;
					break;
				}
				case PF_STATE_SLEEP:
				{
					if (counter == 1)
					{
						// Push
						//push_pixels = true;
						uint16_t temp_fifo = 0x0000;
						for (int i = 0; i < 8; i++)
						{
							temp_fifo <<= 2;
							temp_fifo |= (pixel_fetcher_tile_lo >> 7)| ((pixel_fetcher_tile_hi >> 7) << 1);

							pixel_fetcher_tile_lo <<= 1;
							pixel_fetcher_tile_hi <<= 1;

						}
						out_pixel_fifo |= temp_fifo;
						if (push_stop)
							push_stop = false;
						pixel_fetcher_x += 8;
						pixel_fetcher_state = PF_STATE_GET_TILE;
					}
					break;
				}
				case PF_STATE_PUSH:
				{
					uint16_t temp_fifo = 0x0000;
					for (int i = 0; i < 8; i++)
					{
						temp_fifo <<= 2;
						temp_fifo |= (pixel_fetcher_tile_lo >> 7) | ((pixel_fetcher_tile_hi >> 7) << 1);
						
						pixel_fetcher_tile_lo <<= 1;
						pixel_fetcher_tile_hi <<= 1;
					}
					out_pixel_fifo |= temp_fifo << 16;
					pixel_fetcher_x += 8;
					counter = 1;
					pixel_fetcher_state = PF_STATE_GET_TILE;
					
					break;
				}
			}
			counter++;
			if (counter > 1)
				counter = 0;
			break;
		}
		case PF_MODE_NONE:
		{
			if (none_counter == 0)
			{
				if (pixel_fetcher_x == 0)
				{
					out_palette_fifo = 0x55555555; // PALETTE_NONE
				}
				else
				{
					out_palette_fifo |= 0x5555; // PALETTE_NONE
					if (push_stop)
						push_stop = false;
				}
				pixel_fetcher_x += 8;
			}
			none_counter++;
			if (none_counter > 7)
				none_counter = 0;
			break;
		}
	}
	

	if (draw_x >= 160)
	{
		stat_setmode(STAT_MODE_HBLANK);
	}
}

void hblank()
{
	static bool entered = true;

	// entering H-Blank
	if (entered)
	{
		entered = false;

		// Interrupt request
		int_req_set(STAT_INT_REQ_HBLANK, true);
	}

	// Exiting H-Blank
	if (line_dots == 455)
	{
		entered = true;
		int_req_set(STAT_INT_REQ_HBLANK, false);

		

		if (LY < 143)
			stat_setmode(STAT_MODE_OAM);
		else
			stat_setmode(STAT_MODE_VBLANK);
	}
}

void vblank()
{
	static bool entered = true;

	if (entered)
	{
		entered = false;
		int_req_set(STAT_INT_REQ_VBLANK, true);
		cpu_int_req_set(INT_VBLANK, true);
	}

	if (LY == 153 && line_dots == 455)
	{
		entered = true;
		int_req_set(STAT_INT_REQ_VBLANK, false);
		stat_setmode(STAT_MODE_OAM);
	}
}

int ppu_reset(bool bootskip)
{
	// Reset memory
	memset(vram, 0, sizeof vram);
	memset(oam, 0, sizeof oam);

	LCDC = 0x00;
	STAT = 0x00;
	SCY = 0x00;
	SCX = 0x00;
	LY = 0x00;
	LYC = 0x00;
	BGP = 0x00;
	OBP0 = 0x00;
	OBP1 = 0x00;
	WY = 0x00;
	WX = 0x00;


	if (bootskip)
	{
		LCDC = 0x91;
		BGP = 0xFC;
		OBP0 = 0xFF;
		OBP1 = 0xFF;
	}
	return 0;
}

uint8_t ppu_write(uint16_t addr, uint8_t data)
{
	return bus_write(addr, data, DEV_PPU);
}

uint8_t ppu_read(uint16_t addr)
{
	return bus_read(addr, DEV_PPU);
}

uint8_t vram_write(uint16_t addr, uint8_t data)
{
	// $8000 - $9FFF
	if (addr >= 0x8000 && addr <= 0x9FFF)
	{
		if (!lcdc_getflag(LCDC_LCD_ENABLE) || (lcdc_getflag(LCDC_LCD_ENABLE) && (stat_getmode() != STAT_MODE_DATA))) // Mode is not Data transfer
		{
			vram[addr - 0x8000] = data;
		}
	}
	return 0;
}

uint8_t vram_read(uint16_t addr)
{
	// $8000 - $9FFF
	if (addr >= 0x8000 && addr <= 0x9FFF)
	{
		if (!lcdc_getflag(LCDC_LCD_ENABLE) || (lcdc_getflag(LCDC_LCD_ENABLE) && (stat_getmode() != STAT_MODE_DATA))) // Mode is not Data transfer
		{
			return vram[addr - 0x8000];
		}
	}
	return 0xFF;
}

uint8_t oam_write(uint16_t addr, uint8_t data, uint8_t device)
{
	// $FE00 - $FE9F
	if (addr >= 0xFE00 && addr <= 0xFE9F) 
	{
		if (device == DEV_DMA)
			oam[addr - 0xFE00] = data;
		else if (!lcdc_getflag(LCDC_LCD_ENABLE) || (lcdc_getflag(LCDC_LCD_ENABLE) && !(stat_getmode() >> 1))) // Mode is V-Blank or H-Blank
		{
			oam[addr - 0xFE00] = data;
		}
	}
	return 0;
}

uint8_t oam_read(uint16_t addr)
{
	if (addr >= 0xFE00 && addr <= 0xFE9F)
	{
		if (!lcdc_getflag(LCDC_LCD_ENABLE) || (lcdc_getflag(LCDC_LCD_ENABLE) && !(stat_getmode() >> 1))) // Mode is V-Blank or H-Blank
		{
			return oam[addr - 0xFE00];
		}
	}
	return 0xFF;
}

uint8_t ppu_register_write(uint16_t addr, uint8_t data)
{
	// $FF40 - $FF4B
	switch (addr) 
	{
		case 0xFF40:
			// Turn screen on if changed
			if (!(LCDC & LCDC_LCD_ENABLE) && (data & LCDC_LCD_ENABLE))
			{
				stat_setmode(STAT_MODE_OAM);
				gameboy_screen_on();
			}
			if ((LCDC & LCDC_LCD_ENABLE) && !(data & LCDC_LCD_ENABLE))
				gameboy_screen_off();
			LCDC = data;
			break;
		case 0xFF41:
			// Bits 0-2 are read only
			STAT = (data & 0xF8) | (STAT & 0x07);
			break;
		case 0xFF42:
			SCY = data;
			break;
		case 0xFF43:
			SCX = data;
			break;
		case 0xFF44:
			// LY is read only
			break;
		case 0xFF45:
			LYC = data;
			break;
		case 0xFF46:
			//DMA = data;
			dma_register_write(addr, data);
			break;
		case 0xFF47:
			BGP = data;
			break;
		case 0xFF48:
			OBP0 = data;
			break;
		case 0xFF49:
			OBP1 = data;
			break;
		case 0xFF4A:
			WY = data;
			break;
		case 0xFF4B:
			WX = data;
			break;
		default:
			break;
	}
	return 0;
}

uint8_t ppu_register_read(uint16_t addr)
{
	// $FF40 - $FF4B
	switch (addr)
	{
	case 0xFF40:
		return LCDC;
	case 0xFF41:
		return STAT;
	case 0xFF42:
		return SCY;
	case 0xFF43:
		return SCX;
	case 0xFF44:
		return LY;
	case 0xFF45:
		return LYC;
	case 0xFF46:
		return dma_register_read(addr);
	case 0xFF47:
		return BGP;
	case 0xFF48:
		return OBP0;
	case 0xFF49:
		return OBP1;
	case 0xFF4A:
		return WY;
	case 0xFF4B:
		return WX;
	default:
		return 0xFF;
	}
}

uint8_t lcdc_getflag(uint8_t flag)
{
	return (LCDC & flag) > 0 ? 1 : 0;
}

uint8_t stat_getflag(uint8_t flag)
{
	return (STAT & flag) > 0 ? 1 : 0;
}

uint8_t stat_setflag(uint8_t flag, bool bSet)
{
	STAT = bSet ? STAT | flag : STAT & ~flag;
	return 0;
}

uint8_t stat_getmode()
{
	return STAT & 0x03;
}

uint8_t stat_setmode(uint8_t mode)
{
	STAT = (STAT & ~0x03) | (mode & 0x03);
	return 0;
}

uint8_t int_req_set(uint8_t flag, bool bSet)
{
	if (int_req == 0 && bSet) // If "No conditions met" changes
	{
		int_check = true;
	}
	int_req = bSet ? int_req | flag : int_req & ~flag;
	return 0;
}

uint8_t cpu_int_req_set(uint8_t flag, bool bSet)
{
	return ppu_write(IF_ADDR, bSet ? ppu_read(IF_ADDR) | flag : ppu_read(IF_ADDR) & ~flag);
}

// Register addresses:
// LCDC	0xFF40
// STAT	0xFF41
// SCY	0xFF42
// SCX	0xFF43
// LY	0xFF44
// LYC	0xFF45
// BGP	0xFF47
// OBP0	0xFF48
// OBP1	0xFF49
// WY	0xFF4A
// WX	0xFF4B
void ppu_get_stats(uint8_t* lcdc, uint8_t* stat, uint8_t* scy, uint8_t* scx, uint8_t* ly, uint8_t* lyc,
	uint8_t* bgp, uint8_t* obp0, uint8_t* obp1, uint8_t* wy, uint8_t* wx)
{
	if (lcdc != NULL)
		*lcdc = LCDC;
	if (stat != NULL)
		*stat = STAT;
	if (scy != NULL)
		*scy = SCY;
	if (scx != NULL)
		*scx = SCX;
	if (ly != NULL)
		*ly = LY;
	if (lyc != NULL)
		*lyc = LYC;
	if (bgp != NULL)
		*bgp = BGP;
	if (obp0 != NULL)
		*obp0 = OBP0;
	if (obp1 != NULL)
		*obp1 = OBP1;
	if (wy != NULL)
		*wy = WY;
	if (wx != NULL)
		*wx = WX;
}
