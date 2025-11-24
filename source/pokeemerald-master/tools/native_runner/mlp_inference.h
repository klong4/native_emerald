#ifndef MLP_INFERENCE_H
#define MLP_INFERENCE_H

#include <stdint.h>

// Predict button bitmask from observation vector (float). Returns uint8_t mask.
uint8_t mlp_predict(const float* obs, int obs_len);

#endif // MLP_INFERENCE_H
