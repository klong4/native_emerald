#include "debug_trace.h"
#include <stdio.h>
#include <string.h>

bool debug_trace_enabled = false;
u32 debug_trace_start_pc = 0x08001000;
u32 debug_trace_end_pc = 0x08001020;
int debug_trace_max_instructions = 100;
static int debug_trace_count = 0;

void debug_trace_init(void) {
    debug_trace_count = 0;
}

bool debug_should_trace(u32 pc) {
    if (!debug_trace_enabled) return false;
    if (debug_trace_count >= debug_trace_max_instructions) return false;
    return (pc >= debug_trace_start_pc && pc < debug_trace_end_pc);
}

void debug_trace_instruction(u32 pc, u32 opcode, bool is_thumb, const char* disasm) {
    if (debug_trace_count >= debug_trace_max_instructions) return;
    
    if (is_thumb) {
        printf("[TRACE] PC=0x%08X | Thumb: 0x%04X | %s\n", pc, opcode & 0xFFFF, disasm ? disasm : "");
    } else {
        printf("[TRACE] PC=0x%08X | ARM: 0x%08X | %s\n", pc, opcode, disasm ? disasm : "");
    }
    
    debug_trace_count++;
    
    if (debug_trace_count == debug_trace_max_instructions) {
        printf("[TRACE] Maximum trace instructions reached\n");
    }
}
