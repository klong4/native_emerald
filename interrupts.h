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

// Input I/O registers
#define REG_KEYINPUT 0x130  // Key Input (Read-only, 0=pressed, 1=released)
#define REG_KEYCNT   0x132  // Key Interrupt Control

// Sound I/O registers (stubs for compatibility)
#define REG_SOUND1CNT_L 0x60  // Channel 1 Sweep register
#define REG_SOUND1CNT_H 0x62  // Channel 1 Duty/Length/Envelope
#define REG_SOUND1CNT_X 0x64  // Channel 1 Frequency/Control
#define REG_SOUND2CNT_L 0x68  // Channel 2 Duty/Length/Envelope
#define REG_SOUND2CNT_H 0x6C  // Channel 2 Frequency/Control
#define REG_SOUND3CNT_L 0x70  // Channel 3 Stop/Wave RAM select
#define REG_SOUND3CNT_H 0x72  // Channel 3 Length/Volume
#define REG_SOUND3CNT_X 0x74  // Channel 3 Frequency/Control
#define REG_SOUND4CNT_L 0x78  // Channel 4 Length/Envelope
#define REG_SOUND4CNT_H 0x7C  // Channel 4 Frequency/Control
#define REG_SOUNDCNT_L  0x80  // Control Stereo/Volume/Enable
#define REG_SOUNDCNT_H  0x82  // DMA Sound Control/Mixing
#define REG_SOUNDCNT_X  0x84  // Sound on/off
#define REG_SOUNDBIAS   0x88  // Sound PWM Control
#define REG_WAVE_RAM    0x90  // Wave RAM (0x90-0x9F, 16 bytes)
#define REG_FIFO_A      0xA0  // DMA Sound FIFO A
#define REG_FIFO_B      0xA4  // DMA Sound FIFO B

typedef struct InterruptState {
    u16 ie;    // Interrupt Enable
    u16 if_flag; // Interrupt Request Flags
    u16 ime;   // Interrupt Master Enable
    u16 dispstat; // Display Status
    u16 vcount; // Vertical Counter
    u16 last_scanline; // Track last scanline to detect transitions
} InterruptState;

void interrupt_init(InterruptState *state);
void interrupt_raise(InterruptState *state, u16 flag);
void interrupt_acknowledge(InterruptState *state, u16 flag);
bool interrupt_check(InterruptState *state);
void interrupt_update_vcount(InterruptState *state, u16 scanline);

#endif
