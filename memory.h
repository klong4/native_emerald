#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

// Forward declaration
typedef struct InterruptState InterruptState;
typedef struct TimerState TimerState;
typedef struct DMAState DMAState;
typedef struct RTCState RTCState;

typedef struct Memory_s {
    u8 *rom;              // ROM data (loaded from file)
    u32 rom_size;         // Actual ROM size
    u8 ewram[EWRAM_SIZE]; // External WRAM (256KB)
    u8 iwram[IWRAM_SIZE]; // Internal WRAM (32KB)
    u8 vram[VRAM_SIZE];   // Video RAM (96KB)
    u8 oam[OAM_SIZE];     // Object Attribute Memory (1KB)
    u8 palette[PALETTE_SIZE]; // Palette RAM (1KB)
    u8 io_regs[IO_SIZE];  // I/O Registers (1KB)
    u8 sram[0x20000];     // Save RAM / Flash (128KB for Pokemon Emerald)
    u16 gpio_data;        // GPIO data register
    u16 gpio_direction;   // GPIO direction register (1=output, 0=input)
    u16 gpio_control;     // GPIO control register
    u8 flash_state;       // Flash command state (0=normal, 1=cmd mode)
    u8 flash_cmd;         // Last flash command
    InterruptState *interrupts; // Pointer to interrupt state
    TimerState *timers;   // Pointer to timer state
    DMAState *dma;        // Pointer to DMA state
    RTCState *rtc;        // Pointer to RTC state
} Memory;

// Initialize memory subsystem
void mem_init(Memory *mem);
void mem_cleanup(Memory *mem);
void mem_set_rom(Memory *mem, u8 *rom, u32 size);
void mem_set_interrupts(Memory *mem, InterruptState *interrupts);
void mem_set_timers(Memory *mem, TimerState *timers);
void mem_set_dma(Memory *mem, DMAState *dma);
void mem_set_rtc(Memory *mem, RTCState *rtc);

// Memory access functions
u32 mem_read32(Memory *mem, u32 addr);
u16 mem_read16(Memory *mem, u32 addr);
u8  mem_read8(Memory *mem, u32 addr);

void mem_write32(Memory *mem, u32 addr, u32 value);
void mem_write16(Memory *mem, u32 addr, u16 value);
void mem_write8(Memory *mem, u32 addr, u8 value);

// Helper to get AI input state
u8 mem_get_ai_input(Memory *mem);
void mem_set_ai_input(Memory *mem, u8 value);

#endif // MEMORY_H
