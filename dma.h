#ifndef DMA_H
#define DMA_H

#include "types.h"

// DMA registers (each channel has 12 bytes)
#define REG_DMA0SAD   0xB0  // Source address
#define REG_DMA0DAD   0xB4  // Destination address
#define REG_DMA0CNT_L 0xB8  // Word count
#define REG_DMA0CNT_H 0xBA  // Control

#define REG_DMA1SAD   0xBC
#define REG_DMA1DAD   0xC0
#define REG_DMA1CNT_L 0xC4
#define REG_DMA1CNT_H 0xC6

#define REG_DMA2SAD   0xC8
#define REG_DMA2DAD   0xCC
#define REG_DMA2CNT_L 0xD0
#define REG_DMA2CNT_H 0xD2

#define REG_DMA3SAD   0xD4
#define REG_DMA3DAD   0xD8
#define REG_DMA3CNT_L 0xDC
#define REG_DMA3CNT_H 0xDE

// DMA control bits
#define DMA_ENABLE      0x8000
#define DMA_IRQ         0x4000
#define DMA_START_MASK  0x3000
#define DMA_START_IMM   0x0000
#define DMA_START_VBL   0x1000
#define DMA_START_HBL   0x2000
#define DMA_START_SPEC  0x3000
#define DMA_32BIT       0x0400
#define DMA_REPEAT      0x0200
#define DMA_SRC_INC     0x0000
#define DMA_SRC_DEC     0x0080
#define DMA_SRC_FIX     0x0100
#define DMA_DST_INC     0x0000
#define DMA_DST_DEC     0x0040
#define DMA_DST_FIX     0x0080
#define DMA_DST_RELOAD  0x00C0

typedef struct Memory Memory;

typedef struct {
    u32 source;
    u32 dest;
    u16 count;
    u16 control;
    bool enabled;
    bool irq_enable;
    bool repeat;
    bool word_transfer;
    u32 internal_source;
    u32 internal_dest;
    u16 internal_count;
} DMAChannel;

typedef struct {
    DMAChannel channels[4];
} DMAState;

void dma_init(DMAState *state);
void dma_write_control(DMAState *state, Memory *mem, int channel, u16 value);
void dma_trigger(DMAState *state, Memory *mem, int trigger_type);
bool dma_is_active(DMAState *state);

#endif // DMA_H
