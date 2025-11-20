#ifndef INPUT_H
#define INPUT_H

#include "types.h"
#include "memory.h"

typedef struct {
    u16 current_keys;
    u16 previous_keys;
} InputState;

void input_init(InputState *input);
void input_update(InputState *input, Memory *mem);
void input_set_ai(InputState *input, u8 ai_buttons);
u16 input_get_keys(InputState *input);

#endif // INPUT_H
