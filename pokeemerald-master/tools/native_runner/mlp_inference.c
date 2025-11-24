#include "mlp_inference.h"
#include <stdlib.h>
#include <string.h>

#ifdef MLP_MODEL_PRESENT
#include "mlp_model.h"

// Simple fixed-point MLP: input -> hidden (ReLU) -> output (linear)
uint8_t mlp_predict(const float* obs, int obs_len)
{
    if (obs_len != MLP_NUM_INPUT) return 0;
    int32_t x_q[MLP_NUM_INPUT];
    for (int i = 0; i < MLP_NUM_INPUT; ++i)
        x_q[i] = (int32_t)roundf(obs[i] * MLP_SCALE);

    int32_t hidden[MLP_NUM_HIDDEN];
    for (int j = 0; j < MLP_NUM_HIDDEN; ++j) {
        int32_t acc = MLP_B1[j];
        int idx_base = j * MLP_NUM_INPUT;
        for (int i = 0; i < MLP_NUM_INPUT; ++i) {
            acc += (int32_t)MLP_W1[idx_base + i] * x_q[i];
        }
        // acc currently in scale^2; divide by MLP_SCALE to bring back to scale
        acc /= MLP_SCALE;
        if (acc < 0) acc = 0; // ReLU
        hidden[j] = acc;
    }

    int32_t out[MLP_NUM_OUTPUT];
    for (int k = 0; k < MLP_NUM_OUTPUT; ++k) {
        int32_t acc = MLP_B2[k];
        int idx_base = k * MLP_NUM_HIDDEN;
        for (int j = 0; j < MLP_NUM_HIDDEN; ++j) {
            acc += (int32_t)MLP_W2[idx_base + j] * hidden[j];
        }
        // bring back to scale
        acc /= MLP_SCALE;
        out[k] = acc;
    }

    // Convert outputs to binary actions (threshold at 0)
    uint8_t mask = 0;
    for (int k = 0; k < MLP_NUM_OUTPUT && k < 8; ++k) {
        if (out[k] > 0) mask |= (1 << k);
    }
    return mask;
}

#else

uint8_t mlp_predict(const float* obs, int obs_len)
{
    (void)obs; (void)obs_len;
    // No model present; return no buttons
    return 0;
}

#endif
