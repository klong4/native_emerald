#ifndef TIMER_H
#define TIMER_H

#include "types.h"

// Forward declaration
typedef struct InterruptState InterruptState;

// Timer registers
#define REG_TM0CNT_L  0x100  // Timer 0 Counter
#define REG_TM0CNT_H  0x102  // Timer 0 Control
#define REG_TM1CNT_L  0x104
#define REG_TM1CNT_H  0x106
#define REG_TM2CNT_L  0x108
#define REG_TM2CNT_H  0x10A
#define REG_TM3CNT_L  0x10C
#define REG_TM3CNT_H  0x10E

// Timer control bits
#define TIMER_ENABLE    0x80
#define TIMER_IRQ       0x40
#define TIMER_CASCADE   0x04
#define TIMER_FREQ_MASK 0x03

typedef struct Timer {
    u16 counter;      // Current counter value
    u16 reload;       // Reload value
    u16 control;      // Control register
    bool enabled;     // Timer enabled
    bool irq_enable;  // IRQ enabled
    bool cascade;     // Cascade mode
    u32 prescaler;    // Prescaler value (1, 64, 256, 1024)
    u32 clock;        // Internal clock counter
} Timer;

typedef struct TimerState {
    Timer timers[4];
} TimerState;

void timer_init(TimerState *state);
void timer_update(TimerState *state, u32 cycles, InterruptState *interrupts);
void timer_write_control(TimerState *state, int timer_id, u16 value);
void timer_write_reload(TimerState *state, int timer_id, u16 value);
u16 timer_read_counter(TimerState *state, int timer_id);
u16 timer_read_control(TimerState *state, int timer_id);

#endif // TIMER_H
