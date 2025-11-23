#include "input.h"
#include "interrupts.h"
#include <string.h>

// GBA button bit mapping (for KEYINPUT register)
// Bit 0: A
// Bit 1: B
// Bit 2: Select
// Bit 3: Start
// Bit 4: Right
// Bit 5: Left
// Bit 6: Up
// Bit 7: Down
// Bit 8: R
// Bit 9: L

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
    
    // Write to GBA KEYINPUT register (0x04000130)
    // GBA uses active-LOW: 0=pressed, 1=released
    // So we invert the bits: if ai_input bit is 1 (pressed), KEYINPUT bit should be 0
    u16 gba_keyinput = ~((u16)input->current_keys) & 0x03FF;  // Only lower 10 bits used
    
    // Write directly to io_regs (bypassing normal write protection)
    mem->io_regs[0x130] = gba_keyinput & 0xFF;
    mem->io_regs[0x131] = (gba_keyinput >> 8) & 0xFF;
    
    // Check for keypad interrupt (KEYCNT register at 0x132)
    u16 keycnt = mem->io_regs[0x132] | (mem->io_regs[0x133] << 8);
    if (keycnt & 0x4000) { // Bit 14: IRQ Enable
        u16 selected_keys = keycnt & 0x03FF;
        bool irq_condition = (keycnt & 0x8000) ? 1 : 0; // Bit 15: AND/OR mode
        
        if (irq_condition) {
            // AND mode: trigger if ALL selected keys are pressed
            if (selected_keys && (gba_keyinput & selected_keys) == 0) {
                if (mem->interrupts) {
                    interrupt_raise(mem->interrupts, INT_KEYPAD);
                }
            }
        } else {
            // OR mode: trigger if ANY selected key is pressed
            if ((~gba_keyinput) & selected_keys) {
                if (mem->interrupts) {
                    interrupt_raise(mem->interrupts, INT_KEYPAD);
                }
            }
        }
    }
}

void input_set_ai(InputState *input, u8 ai_buttons) {
    if (!input) return;
    input->current_keys = ai_buttons;
}

u16 input_get_keys(InputState *input) {
    return input ? input->current_keys : 0;
}
