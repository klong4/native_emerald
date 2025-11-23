#include "timer.h"
#include "interrupts.h"
#include <string.h>

void timer_init(TimerState *state) {
    memset(state, 0, sizeof(TimerState));
    
    for (int i = 0; i < 4; i++) {
        state->timers[i].prescaler = 1;
    }
}

static u32 get_prescaler(u16 control) {
    switch (control & TIMER_FREQ_MASK) {
        case 0: return 1;
        case 1: return 64;
        case 2: return 256;
        case 3: return 1024;
    }
    return 1;
}

void timer_update(TimerState *state, u32 cycles, InterruptState *interrupts) {
    for (int i = 0; i < 4; i++) {
        Timer *timer = &state->timers[i];
        
        if (!timer->enabled) continue;
        
        // Cascade mode: increment when previous timer overflows
        if (timer->cascade && i > 0) {
            continue; // Handled when previous timer overflows
        }
        
        // Add cycles to internal clock
        timer->clock += cycles;
        
        // Check if prescaler threshold reached
        while (timer->clock >= timer->prescaler) {
            timer->clock -= timer->prescaler;
            timer->counter++;
            
            // Check for overflow
            if (timer->counter == 0) {
                // Reload
                timer->counter = timer->reload;
                
                // Trigger IRQ if enabled
                if (timer->irq_enable && interrupts) {
                    interrupt_raise(interrupts, INT_TIMER0 << i);
                }
                
                // Increment cascade timer if enabled
                if (i < 3 && state->timers[i + 1].cascade && state->timers[i + 1].enabled) {
                    state->timers[i + 1].counter++;
                    if (state->timers[i + 1].counter == 0) {
                        state->timers[i + 1].counter = state->timers[i + 1].reload;
                        if (state->timers[i + 1].irq_enable && interrupts) {
                            interrupt_raise(interrupts, INT_TIMER0 << (i + 1));
                        }
                    }
                }
            }
        }
    }
}

void timer_write_control(TimerState *state, int timer_id, u16 value) {
    if (timer_id < 0 || timer_id >= 4) return;
    
    Timer *timer = &state->timers[timer_id];
    
    bool was_enabled = timer->enabled;
    timer->control = value;
    timer->enabled = (value & TIMER_ENABLE) != 0;
    timer->irq_enable = (value & TIMER_IRQ) != 0;
    timer->cascade = (value & TIMER_CASCADE) != 0;
    timer->prescaler = get_prescaler(value);
    
    // If timer just got enabled, reload counter
    if (timer->enabled && !was_enabled) {
        timer->counter = timer->reload;
        timer->clock = 0;
    }
}

void timer_write_reload(TimerState *state, int timer_id, u16 value) {
    if (timer_id < 0 || timer_id >= 4) return;
    
    state->timers[timer_id].reload = value;
    
    // If timer is disabled, also update counter
    if (!state->timers[timer_id].enabled) {
        state->timers[timer_id].counter = value;
    }
}

u16 timer_read_counter(TimerState *state, int timer_id) {
    if (timer_id < 0 || timer_id >= 4) return 0;
    return state->timers[timer_id].counter;
}

u16 timer_read_control(TimerState *state, int timer_id) {
    if (timer_id < 0 || timer_id >= 4) return 0;
    return state->timers[timer_id].control;
}
