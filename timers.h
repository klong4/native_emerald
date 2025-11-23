#ifndef TIMERS_H
#define TIMERS_H

#include "types.h"
#include "interrupts.h"

// Timer control bits
#define TIMER_ENABLE    (1 << 7)
#define TIMER_IRQ       (1 << 6)
#define TIMER_CASCADE   (1 << 2)
#define TIMER_PRESCALER_MASK 0x03

typedef struct {
    u16 reload;      // Reload value
    u16 counter;     // Current counter value
    u16 control;     // Control register
    u32 cycles;      // Cycle accumulator
    bool overflow;   // Overflow flag for cascading
} Timer;

typedef struct {
    Timer timers[4];
} TimerState;

void timer_init(TimerState *state);
void timer_update(TimerState *state, u32 cycles, InterruptState *interrupts);
void timer_write_reload(TimerState *state, int timer_num, u16 value);
void timer_write_control(TimerState *state, int timer_num, u16 value);
u16 timer_read_counter(TimerState *state, int timer_num);

#endif // TIMERS_H
