#include "bios.h"
#include <string.h>
#include <stdio.h>

// GBA BIOS stub - provides minimal boot functionality
// Real BIOS is 16KB, we provide essential parts

static u8 bios_memory[0x4000]; // 16KB BIOS region

void bios_init(void) {
    memset(bios_memory, 0, sizeof(bios_memory));
    
    // Write ARM exception vectors at the start of BIOS
    // Keep vectors simple - just loop on exceptions (game shouldn't hit these normally)
    u32 vectors_v2[] = {
        0xEA000000,  // 0x00: Reset - b +0 (infinite loop - should not be reached)
        0xEA000000,  // 0x04: Undefined instruction - b +0
        0xEA000000,  // 0x08: SWI - b +0 (SWI calls handled by HLE in cpu_core)
        0xEA000000,  // 0x0C: Prefetch abort - b +0
        0xEA000000,  // 0x10: Data abort - b +0
        0xEA000000,  // 0x14: Reserved - b +0
        0xEA000006,  // 0x18: IRQ - b irq_handler (+6 words = 0x34)
        0xEA000000,  // 0x1C: FIQ - b +0
    };
    
    for (int i = 0; i < 8; i++) {
        bios_memory[i*4 + 0] = (vectors_v2[i] >> 0) & 0xFF;
        bios_memory[i*4 + 1] = (vectors_v2[i] >> 8) & 0xFF;
        bios_memory[i*4 + 2] = (vectors_v2[i] >> 16) & 0xFF;
        bios_memory[i*4 + 3] = (vectors_v2[i] >> 24) & 0xFF;
    }
    
    // IRQ handler at 0x34 (referenced by vector at 0x18)
    // This implements the BIOS IRQ handler that calls the game's handler
    u32 irq_handler_v2[] = {
        0xE92D500F,  // 0x2C: STMFD SP!, {R0-R3,R12,LR}  ; Save context
        0xE59F1010,  // 0x30: LDR R1, [PC, #16]          ; Load address 0x03007FFC from literal pool (PC+8+16 = 0x30+8+10=0x48)
        0xE5911000,  // 0x34: LDR R1, [R1]                ; R1 = handler pointer from [0x03007FFC]
        0xE1A0E00F,  // 0x38: MOV LR, PC                  ; Save return address  
        0xE92D500F,  // 0x34: STMFD SP!, {R0-R3,R12,LR}  ; Save context
        0xE59F1010,  // 0x38: LDR R1, [PC, #16]          ; Load address 0x03007FFC from literal pool
        0xE5911000,  // 0x3C: LDR R1, [R1]                ; R1 = handler pointer from [0x03007FFC]
        0xE1A0E00F,  // 0x40: MOV LR, PC                  ; Save return address  
        0xE12FFF11,  // 0x44: BX R1                        ; Call handler (handles ARM/Thumb)
        0xE8BD500F,  // 0x48: LDMFD SP!, {R0-R3,R12,LR}  ; Restore context
        0xE25EF004,  // 0x4C: SUBS PC, LR, #4             ; Return from IRQ
        0x03007FFC,  // 0x50: Literal pool: address of interrupt vector
    };
    
    for (int i = 0; i < 8; i++) {
        int offset = 0x34 + i*4;
        bios_memory[offset + 0] = (irq_handler_v2[i] >> 0) & 0xFF;
        bios_memory[offset + 1] = (irq_handler_v2[i] >> 8) & 0xFF;
        bios_memory[offset + 2] = (irq_handler_v2[i] >> 16) & 0xFF;
        bios_memory[offset + 3] = (irq_handler_v2[i] >> 24) & 0xFF;
    }
    
    // BIOS data area used by games
    bios_memory[0xDC] = 0x00;
    bios_memory[0xDD] = 0x00;
    bios_memory[0xDE] = 0x00;
    bios_memory[0xDF] = 0x00;
    bios_memory[0xE0] = 0x01;  // Interrupts initialized flag
    
    // Since we skip BIOS boot and jump directly to ROM, 
    // we don't need complex BIOS boot code.
    // If a game tries to call BIOS reset (BX to 0x00), 
    // it will hit the vector which loops, but we handle
    // SWI calls via HLE in cpu_core.c
    
    // Fill remaining BIOS with ARM NOP (0xE1A00000 = mov r0, r0)
    for (int i = 0x20; i < 0x4000; i += 4) {
        bios_memory[i + 0] = 0x00;
        bios_memory[i + 1] = 0x00;
        bios_memory[i + 2] = 0xA0;
        bios_memory[i + 3] = 0xE1;
    }
}

u8 bios_read8(u32 addr) {
    if (addr < 0x4000) {
        return bios_memory[addr];
    }
    return 0;
}

u16 bios_read16(u32 addr) {
    if (addr < 0x3FFF) {
        return bios_memory[addr] | (bios_memory[addr + 1] << 8);
    }
    return 0;
}

u32 bios_read32(u32 addr) {
    if (addr < 0x3FFD) {
        return bios_memory[addr] | 
               (bios_memory[addr + 1] << 8) | 
               (bios_memory[addr + 2] << 16) | 
               (bios_memory[addr + 3] << 24);
    }
    return 0;
}

void bios_write8(u32 addr, u8 value) {
    // BIOS is read-only in hardware, but allow writes to flags area
    if (addr >= 0xDC && addr < 0x100) {
        bios_memory[addr] = value;
    }
}

void bios_write16(u32 addr, u16 value) {
    if (addr >= 0xDC && addr < 0xFF) {
        bios_memory[addr] = value & 0xFF;
        bios_memory[addr + 1] = (value >> 8) & 0xFF;
    }
}

void bios_write32(u32 addr, u32 value) {
    if (addr >= 0xDC && addr < 0xFD) {
        bios_memory[addr] = value & 0xFF;
        bios_memory[addr + 1] = (value >> 8) & 0xFF;
        bios_memory[addr + 2] = (value >> 16) & 0xFF;
        bios_memory[addr + 3] = (value >> 24) & 0xFF;
    }
}
