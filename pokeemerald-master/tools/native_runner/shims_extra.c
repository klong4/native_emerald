#include "shims_extra.h"
#include <string.h>

void CpuFastSet(const void *src, void *dst, int count)
{
    // count is number of 32-bit words in GBA's CpuFastSet semantics; approximate with bytes
    memcpy(dst, src, (size_t)count);
}

void CpuSet(const void *src, void *dst, int control)
{
    // control semantics vary; approximate by copying a small amount
    (void)control;
    memcpy(dst, src, 4);
}

void SoftReset(void)
{
}

int m4aMPlayOpen(void)
{
    return 0;
}

void m4aMPlayStop(int ch)
{
    (void)ch;
}

void SoundDriverInit(void)
{
}

void SoundDriverMain(void)
{
}
// End of file
