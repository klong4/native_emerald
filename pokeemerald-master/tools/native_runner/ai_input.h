#ifndef AI_INPUT_H
#define AI_INPUT_H

#include <stdint.h>

// Simple AI input API used by the native runner. The AI policy or external tool
// should call `ai_set_buttons` each frame to set the desired button bitmask.

// Returns a bitmask of buttons currently set by the AI.
uint8_t ai_read_buttons(void);
// Sets the button bitmask for the AI (called by embedded policy or host).
void ai_set_buttons(uint8_t b);

#endif // AI_INPUT_H
