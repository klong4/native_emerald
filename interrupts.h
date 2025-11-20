#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "types.h"

// Interrupt flags
#define INT_VBLANK   0x0001
#define INT_HBLANK   0x0002
#define INT_VCOUNT   0x0004
#define INT_TIMER0   0x0008
#define INT_TIMER1   0x0010
#define INT_TIMER2   0x0020
#define INT_TIMER3   0x0040
#define INT_SERIAL   0x0080
#define INT_DMA0     0x0100
#define INT_DMA1     0x0200
#define INT_DMA2     0x0400
#define INT_DMA3     0x0800
#define INT_KEYPAD   0x1000
#define INT_GAMEPAK  0x2000

// Interrupt I/O registers
#define REG_IE       0x200  // Interrupt Enable
#define REG_IF       0x202  // Interrupt Request Flags
#define REG_IME      0x208  // Interrupt Master Enable

typedef struct InterruptState {
    u16 ie;    // Interrupt Enable
    u16 if_flag; // Interrupt Request Flags
    u16 ime;   // Interrupt Master Enable
    u16 dispstat; // Display Status
    u16 vcount; // Vertical Counter
} InterruptState;

void interrupt_init(InterruptState *state);
void interrupt_raise(InterruptState *state, u16 flag);
void interrupt_acknowledge(InterruptState *state, u16 flag);
bool interrupt_check(InterruptState *state);
void interrupt_update_vcount(InterruptState *state, u16 scanline);

#endif
