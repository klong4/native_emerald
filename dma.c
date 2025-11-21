#include "dma.h"
#include "memory.h"
#include "interrupts.h"
#include <string.h>

void dma_init(DMAState *state) {
    memset(state, 0, sizeof(DMAState));
}

static void dma_execute(DMAChannel *dma, Memory *mem) {
    if (!dma->enabled) return;
    
    // Get transfer parameters
    u32 src = dma->internal_source;
    u32 dst = dma->internal_dest;
    u16 count = dma->internal_count;
    
    if (count == 0) {
        count = (dma - (DMAChannel*)0 == 3) ? 0x10000 : 0x4000; // Max count
    }
    
    // Get source/dest increment modes
    int src_step = 0;
    int dst_step = 0;
    int transfer_size = dma->word_transfer ? 4 : 2;
    
    u16 ctrl = dma->control;
    
    // Source address control
    switch ((ctrl >> 7) & 3) {
        case 0: src_step = transfer_size; break;  // Increment
        case 1: src_step = -transfer_size; break; // Decrement
        case 2: src_step = 0; break;               // Fixed
    }
    
    // Dest address control
    switch ((ctrl >> 5) & 3) {
        case 0: dst_step = transfer_size; break;  // Increment
        case 1: dst_step = -transfer_size; break; // Decrement
        case 2: dst_step = 0; break;               // Fixed
        case 3: dst_step = transfer_size; break;  // Increment/Reload
    }
    
    // Perform transfer
    for (u16 i = 0; i < count; i++) {
        if (dma->word_transfer) {
            u32 value = mem_read32(mem, src);
            mem_write32(mem, dst, value);
        } else {
            u16 value = mem_read16(mem, src);
            mem_write16(mem, dst, value);
        }
        
        src += src_step;
        dst += dst_step;
    }
    
    // Update internal registers
    dma->internal_source = src;
    dma->internal_dest = dst;
    
    // Handle repeat mode
    if (!dma->repeat) {
        dma->enabled = false;
        dma->control &= ~DMA_ENABLE;
    } else {
        // Reload dest if in reload mode
        if (((ctrl >> 5) & 3) == 3) {
            dma->internal_dest = dma->dest;
        }
        dma->internal_count = dma->count;
    }
    
    // Trigger IRQ if enabled
    if (dma->irq_enable) {
        // IRQ will be handled by caller
    }
}

void dma_write_control(DMAState *state, Memory *mem, int channel, u16 value) {
    if (channel < 0 || channel >= 4) return;
    
    DMAChannel *dma = &state->channels[channel];
    
    bool was_enabled = dma->enabled;
    
    dma->control = value;
    dma->enabled = (value & DMA_ENABLE) != 0;
    dma->irq_enable = (value & DMA_IRQ) != 0;
    dma->repeat = (value & DMA_REPEAT) != 0;
    dma->word_transfer = (value & DMA_32BIT) != 0;
    
    // If DMA just got enabled, initialize internal registers
    if (dma->enabled && !was_enabled) {
        dma->internal_source = dma->source;
        dma->internal_dest = dma->dest;
        dma->internal_count = dma->count;
        
        // Check start timing
        u16 start_mode = (value & DMA_START_MASK) >> 12;
        
        // Immediate start (start_mode == 0)
        if (start_mode == 0) {
            dma_execute(dma, mem);
        }
        // Other modes (VBlank, HBlank) will be triggered externally
    }
}

void dma_trigger(DMAState *state, Memory *mem, int trigger_type) {
    // trigger_type: 1=VBlank, 2=HBlank, 3=Special
    for (int i = 0; i < 4; i++) {
        DMAChannel *dma = &state->channels[i];
        if (!dma->enabled) continue;
        
        u16 start_mode = (dma->control & DMA_START_MASK) >> 12;
        
        if (start_mode == trigger_type) {
            dma_execute(dma, mem);
        }
    }
}

bool dma_is_active(DMAState *state) {
    for (int i = 0; i < 4; i++) {
        if (state->channels[i].enabled) return true;
    }
    return false;
}
