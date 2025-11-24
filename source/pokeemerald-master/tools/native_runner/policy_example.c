#include "policy_example.h"
#include "ai_input.h"
#include "mlp_inference.h"

// Simple example: press "A" (bit 0) every 60 frames for one frame.
static int s_frameCount = 0;

void policy_tick(void)
{
    s_frameCount++;
    // Example usage: call embedded MLP if available. Provide a tiny dummy observation.
    float obs[8];
    int obs_len = 8;
    for (int i = 0; i < obs_len; ++i) obs[i] = 0.0f;
    // If model present, mlp_predict will return a mask; otherwise returns 0.
    uint8_t mask = mlp_predict(obs, obs_len);
    if (mask) {
        ai_set_buttons(mask);
    } else {
        // fallback behavior: pulse A every 60 frames
        if (s_frameCount % 60 == 0) ai_set_buttons(0x1);
        else ai_set_buttons(0x0);
    }
}
