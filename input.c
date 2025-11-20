#include "input.h"
#include <string.h>

void input_init(InputState *input) {
    if (!input) return;
    input->current_keys = 0;
    input->previous_keys = 0;
}

void input_update(InputState *input, Memory *mem) {
    if (!input || !mem) return;
    
    input->previous_keys = input->current_keys;
    
    // Read AI input from gAiInputState
    u8 ai_input = mem_get_ai_input(mem);
    input->current_keys = ai_input;
    
    // Write to GBA input register (0x04000130 - KEYINPUT)
    // GBA uses active-LOW, so we invert
    u16 gba_keys = ~input->current_keys;
    mem_write16(mem, 0x04000130, gba_keys);
}

void input_set_ai(InputState *input, u8 ai_buttons) {
    if (!input) return;
    input->current_keys = ai_buttons;
}

u16 input_get_keys(InputState *input) {
    return input ? input->current_keys : 0;
}
