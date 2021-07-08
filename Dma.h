#ifndef DMA_CODE
#define DMA_CODE

void dma_clock();
void dma_reset();
void dma_init();

uint8_t dma_read(uint16_t addr);
uint8_t dma_write(uint16_t addr, uint8_t data);
uint8_t dma_register_write(uint16_t addr, uint8_t data);
uint8_t dma_register_read(uint16_t addr);

#endif // #ifndef DMA_CODE