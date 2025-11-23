#ifndef DEBUG_TRACE_H
#define DEBUG_TRACE_H

#include "types.h"
#include <stdbool.h>

// Debug tracing configuration
extern bool debug_trace_enabled;
extern u32 debug_trace_start_pc;
extern u32 debug_trace_end_pc;
extern int debug_trace_max_instructions;

// Initialize debug tracing
void debug_trace_init(void);

// Log a single instruction execution
void debug_trace_instruction(u32 pc, u32 opcode, bool is_thumb, const char* disasm);

// Check if we should trace this PC
bool debug_should_trace(u32 pc);

#endif // DEBUG_TRACE_H
