#ifndef CPU_CORE_H
#define CPU_CORE_H

#include "types.h"
#include "memory.h"

// Forward declaration
typedef struct InterruptState InterruptState;

// CPU Status flags
#define FLAG_N  (1 << 31)  // Negative
#define FLAG_Z  (1 << 30)  // Zero
#define FLAG_C  (1 << 29)  // Carry
#define FLAG_V  (1 << 28)  // Overflow
#define FLAG_T  (1 <<  5)  // Thumb mode
#define FLAG_I  (1 <<  7)  // IRQ disable

typedef struct {
    u32 r[16];           // R0-R15 (R13=SP, R14=LR, R15=PC)
    u32 cpsr;            // Current Program Status Register
    u32 spsr;            // Saved PSR
    bool thumb_mode;     // true = Thumb, false = ARM
    u64 cycles;          // Total cycles executed
    bool halted;         // CPU halted flag
} ARM7TDMI;

// Initialize CPU
void cpu_init(ARM7TDMI *cpu);
void cpu_reset(ARM7TDMI *cpu);

// Execution
void cpu_execute_frame(ARM7TDMI *cpu, Memory *mem, InterruptState *interrupts);
u32 cpu_step(ARM7TDMI *cpu, Memory *mem);
void cpu_handle_interrupt(ARM7TDMI *cpu, Memory *mem);

// Helper functions
void cpu_set_flag(ARM7TDMI *cpu, u32 flag);
void cpu_clear_flag(ARM7TDMI *cpu, u32 flag);
bool cpu_get_flag(ARM7TDMI *cpu, u32 flag);

#endif // CPU_CORE_H
