#ifndef SHIMS_EXTRA_H
#define SHIMS_EXTRA_H

#include <stdint.h>

void CpuFastSet(const void *src, void *dst, int count);
void CpuSet(const void *src, void *dst, int control);
void SoftReset(void);

// Minimal sound stubs
int m4aMPlayOpen(void);
void m4aMPlayStop(int ch);
void SoundDriverInit(void);
void SoundDriverMain(void);

#endif // SHIMS_EXTRA_H
