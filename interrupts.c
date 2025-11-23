#include "interrupts.h"
#include <string.h>

void interrupt_init(InterruptState *state) {
    if (!state) return;
    
    state->ie = 0;
    state->if_flag = 0;
    state->ime = 0;
    state->dispstat = 0;
    state->vcount = 0;
    state->last_scanline = 0;
}

void interrupt_raise(InterruptState *state, u16 flag) {
    if (!state) return;
    state->if_flag |= flag;
}

void interrupt_acknowledge(InterruptState *state, u16 flag) {
    if (!state) return;
    state->if_flag &= ~flag;
}

bool interrupt_check(InterruptState *state) {
    if (!state) return false;
    
    // Check if interrupts are globally enabled
    if (!(state->ime & 1)) return false;
    
    // Check if any enabled interrupt is pending
    return (state->ie & state->if_flag) != 0;
}

void interrupt_update_vcount(InterruptState *state, u16 scanline) {
    if (!state) return;
    
    u16 prev_scanline = state->last_scanline;
    state->vcount = scanline;
    state->last_scanline = scanline;
    
    // Check VCount match
    u8 vcount_setting = (state->dispstat >> 8) & 0xFF;
    if (scanline == vcount_setting) {
        state->dispstat |= 0x04; // VCount flag
        if (state->dispstat & 0x20) { // VCount IRQ enable
            interrupt_raise(state, INT_VCOUNT);
        }
    } else {
        state->dispstat &= ~0x04;
    }
    
    // VBlank flag (scanlines 160-227)
    // Only raise interrupt on TRANSITION to scanline 160
    if (scanline == 160 && prev_scanline != 160) {
        state->dispstat |= 0x01; // Set VBlank flag
        // Raise VBlank interrupt (will be checked against IE by interrupt_check)
        interrupt_raise(state, INT_VBLANK);
    } else if (scanline == 0) {
        state->dispstat &= ~0x01; // Clear VBlank flag
    }
    
    // HBlank flag (set during HBlank period)
    // For now, we'll trigger HBlank at the end of each scanline
}
