// Stub files - minimal implementations

// save_state.h
#ifndef SAVE_STATE_H
#define SAVE_STATE_H
#include "types.h"
void save_state(const void *state, const char *filename);
void load_state(void *state, const char *filename);
#endif

// audio_stub.h
#ifndef AUDIO_STUB_H
#define AUDIO_STUB_H
#include "types.h"
typedef struct { u32 dummy; } AudioState;
void audio_init(AudioState *a);
void audio_update(AudioState *a);
void audio_cleanup(void);
#endif

// python_bridge.h
#ifndef PYTHON_BRIDGE_H
#define PYTHON_BRIDGE_H
#include "types.h"
void python_bridge_init(void *emu);
void python_bridge_update(void *emu);
#endif
