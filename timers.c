#include "timers.h"
#include <string.h>

// Timer prescaler values (cycles per tick)
static const u32 prescaler_cycles[] = {1, 64, 256, 1024};

void timer_init(TimerState *state) {
    memset(state, 0, sizeof(TimerState));
}

void timer_update(TimerState *state, u32 cycles, InterruptState *interrupts) {
    for (int i = 0; i < 4; i++) {
        Timer *timer = &state->timers[i];
        
        // Skip if timer is not enabled
        if (!(timer->control & TIMER_ENABLE)) {
            continue;
        }
        
        // Check if this is a cascade timer
        bool cascade = (timer->control & TIMER_CASCADE) && (i > 0);
        
        if (cascade) {
            // Cascade mode: increment when previous timer overflows
            if (state->timers[i-1].overflow) {
                timer->counter++;
                if (timer->counter == 0) {
                    // Overflow
                    timer->counter = timer->reload;
                    timer->overflow = true;
                    
                    // Raise interrupt if enabled
                    if (timer->control & TIMER_IRQ) {
                        interrupt_raise(interrupts, 1 << (3 + i)); // Timer 0-3 interrupts
                    }
                } else {
                    timer->overflow = false;
                }
            }
        } else {
            // Normal mode: increment based on prescaler
            u32 prescaler = prescaler_cycles[timer->control & TIMER_PRESCALER_MASK];
            timer->cycles += cycles;
            
            while (timer->cycles >= prescaler) {
                timer->cycles -= prescaler;
                timer->counter++;
                
                if (timer->counter == 0) {
                    // Overflow
                    timer->counter = timer->reload;
                    timer->overflow = true;
                    
                    // Raise interrupt if enabled
                    if (timer->control & TIMER_IRQ) {
                        interrupt_raise(interrupts, 1 << (3 + i)); // Timer 0-3 interrupts
                    }
                } else {
                    timer->overflow = false;
                }
            }
        }
    }
    
    // Clear overflow flags at end of update
    for (int i = 0; i < 4; i++) {
        state->timers[i].overflow = false;
    }
}

void timer_write_reload(TimerState *state, int timer_num, u16 value) {
    if (timer_num < 0 || timer_num >= 4) return;
    state->timers[timer_num].reload = value;
}

void timer_write_control(TimerState *state, int timer_num, u16 value) {
    if (timer_num < 0 || timer_num >= 4) return;
    
    Timer *timer = &state->timers[timer_num];
    bool was_enabled = timer->control & TIMER_ENABLE;
    bool now_enabled = value & TIMER_ENABLE;
    
    timer->control = value;
    
    // If timer was just enabled, reload the counter
    if (!was_enabled && now_enabled) {
        timer->counter = timer->reload;
        timer->cycles = 0;
        timer->overflow = false;
    }
}

u16 timer_read_counter(TimerState *state, int timer_num) {
    if (timer_num < 0 || timer_num >= 4) return 0;
    return state->timers[timer_num].counter;
}
