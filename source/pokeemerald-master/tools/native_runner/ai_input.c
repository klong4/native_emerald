#include "ai_input.h"

#if defined(_MSC_VER)
// MSVC doesn't provide stdatomic in the same way; use a volatile fallback for this
// tiny cross-thread flag (native runner uses single-threaded policy by default).
static volatile uint8_t s_aiButtons = 0;

uint8_t ai_read_buttons(void)
{
    return (uint8_t)s_aiButtons;
}

void ai_set_buttons(uint8_t b)
{
    s_aiButtons = b;
}
#else
#include <stdatomic.h>

static atomic_uint_fast8_t s_aiButtons = 0;

uint8_t ai_read_buttons(void)
{
    return (uint8_t)atomic_load(&s_aiButtons);
}

void ai_set_buttons(uint8_t b)
{
    atomic_store(&s_aiButtons, b);
}
#endif
