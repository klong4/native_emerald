#include "stubs.h"
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

static SDL_AudioDeviceID s_audio_device = 0;

// Audio callback
static void audio_callback(void *userdata, u8 *stream, int len) {
    memset(stream, 0, len); // Silence for now
}

// save_state.c
void save_state(const void *state, const char *filename) {
    (void)state; (void)filename;
    printf("save_state stub\n");
}

void load_state(void *state, const char *filename) {
    (void)state; (void)filename;
    printf("load_state stub\n");
}

// timer.c
void timer_init(TimerState *t) {
    (void)t;
}

void timer_update(TimerState *t) {
    (void)t;
}

// audio_stub.c
void audio_init(AudioState *a) {
    (void)a;
    
    if (s_audio_device > 0) return;
    
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 32768; // GBA sample rate
    want.format = AUDIO_S16LSB;
    want.channels = 2;
    want.samples = 512;
    want.callback = audio_callback;
    
    s_audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (s_audio_device == 0) {
        printf("Failed to open audio: %s\n", SDL_GetError());
        return;
    }
    
    SDL_PauseAudioDevice(s_audio_device, 0);
    printf("Audio initialized: %d Hz, %d channels\n", have.freq, have.channels);
}

void audio_update(AudioState *a) {
    (void)a;
}

void audio_cleanup(void) {
    if (s_audio_device > 0) {
        SDL_CloseAudioDevice(s_audio_device);
        s_audio_device = 0;
    }
}

// python_bridge.c
void python_bridge_init(void *emu) {
    (void)emu;
}

void python_bridge_update(void *emu) {
    (void)emu;
}
