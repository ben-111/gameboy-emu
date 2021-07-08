#ifndef PPU_CODE
#define PPU_CODE

void ppu_clock();
int ppu_reset(bool bootskip);

uint8_t ppu_write(uint16_t addr, uint8_t data);
uint8_t ppu_read(uint16_t addr);

uint8_t vram_write(uint16_t addr, uint8_t data);
uint8_t vram_read(uint16_t addr);
uint8_t oam_write(uint16_t addr, uint8_t data, uint8_t device);
uint8_t oam_read(uint16_t addr);
uint8_t ppu_register_write(uint16_t addr, uint8_t data);
uint8_t ppu_register_read(uint16_t addr);

uint8_t lcdc_getflag(uint8_t flag);
uint8_t stat_getflag(uint8_t flag);
uint8_t stat_setflag(uint8_t flag, bool bSet);
uint8_t stat_getmode();
uint8_t stat_setmode(uint8_t mode);
uint8_t int_req_set(uint8_t flag, bool bSet);
uint8_t cpu_int_req_set(uint8_t flag, bool bSet);

//uint8_t pixel_fifo_get();
//uint8_t pixel_fifo_clear();

//uint8_t queue_get(uint16_t* queue);

//uint8_t pixel_fetcher_clock();
//uint8_t oam_search_clock();

void oam_search();
void data_transfer();
void hblank();
void vblank();

enum LCDC_FLAGS {
	LCDC_BG_ENABLE			= (1 << 0),	// 0=BG and Window disabled, 1=BG and Window enabled
	LCDC_OBJ_ENABLE			= (1 << 1),	// 0=Sprites disabled, 1=Sprites enabled
	LCDC_OBJ_SIZE			= (1 << 2),	// 0=8x8, 1=8x16
	LCDC_BG_TILE_MAP		= (1 << 3),	// 0=9800 - 9BFF, 1=9C00 - 9FFF 
	LCDC_BG_TILE_DATA		= (1 << 4),	// 0=8800 - 97FF (signed pointer from base 9000), 1=8000 - 8FFF
	LCDC_WINDOW_ENABLE		= (1 << 5),	// 0=Off, 1=On
	LCDC_WINDOW_TILE_MAP	= (1 << 6),	// 0=9800 - 9BFF, 1=9C00 - 9FFF 
	LCDC_LCD_ENABLE			= (1 << 7)	// 0=Off, 1=On
};

enum STAT_REG_FLAGS {
	STAT_MODE_LOW			= (1 << 0),	// Mode low, read only
	STAT_MODE_HI			= (1 << 1),	// Mode high, read only (mode hi << 1 | mode low = 0 - 3)
	STAT_COINCIDENCE		= (1 << 2),	// 0=LY != LYC, 1=LY == LYC, read only
	STAT_HBLANK_INT			= (1 << 3),	// 1=Enable H-Blank interrupt
	STAT_VBLANK_INT			= (1 << 4),	// 1=Enable V-Blank interrupt
	STAT_OAM_INT			= (1 << 5),	// 1=Enable OAM interrupt
	STAT_COINCIDENCE_INT	= (1 << 6)	// 1=Enable coincidence interrupt
};

enum STAT_MODES {
	STAT_MODE_HBLANK = 0,
	STAT_MODE_VBLANK,
	STAT_MODE_OAM,
	STAT_MODE_DATA
};

enum PALETTE_CODES {
	PALETTE_BGP = 0,
	PALETTE_NONE,
	PALETTE_OBP0,
	PALETTE_OBP1,
	
};

enum STAT_INT_REQS {
	STAT_INT_REQ_COIN	= (1 << 0),
	STAT_INT_REQ_OAM	= (1 << 1),
	STAT_INT_REQ_VBLANK = (1 << 2),
	STAT_INT_REQ_HBLANK = (1 << 3)
};

typedef struct {
	uint8_t x_pos;
	uint8_t index;
} OBJ_REF;

typedef struct {
	uint8_t color : 2, palette : 1, priority : 1;
} FIFO_PIXEL;

enum PF_STATES {
	PF_STATE_GET_TILE = 0,
	PF_STATE_GET_TILE_DATA_LO,
	PF_STATE_GET_TILE_DATA_HI,
	PF_STATE_SLEEP,
	PF_STATE_PUSH
};

enum PF_MODES {
	PF_MODE_BG = 0,
	PF_MODE_WINDOW,
	PF_MODE_OBJ,
	PF_MODE_NONE
};

enum OBJ_ATTRIBUTES {
	OBJ_ATTR_BG_PRIORITY = (1 << 7),// 0=OBJ above BG, 1=OBJ behind colors 1-3
	OBJ_ATTR_Y_FLIP = (1 << 6),		// 1=Flipped
	OBJ_ATTR_X_FLIP = (1 << 5),		// 1=Flipped
	OBJ_ATTR_PALETTE = (1 << 4)		// 0=OBP0, 1=OBP1
};

#define TILE_SIZE 16

void ppu_get_stats(uint8_t* lcdc, uint8_t* stat, uint8_t* scy, uint8_t* scx, uint8_t* ly, uint8_t* lyc,
	uint8_t* bgp, uint8_t* obp0, uint8_t* obp1, uint8_t* wy, uint8_t* wx);

#endif // PPU_CODE