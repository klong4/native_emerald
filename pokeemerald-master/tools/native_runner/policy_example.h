#ifndef POLICY_EXAMPLE_H
#define POLICY_EXAMPLE_H

#include <stdint.h>

// Example embedded policy: called each frame to update AI inputs via ai_set_buttons().
void policy_tick(void);

#endif // POLICY_EXAMPLE_H
