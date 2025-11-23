#ifndef BIOS_H
#define BIOS_H

#include "types.h"

// Initialize BIOS emulation
void bios_init(void);

// BIOS memory access
u8 bios_read8(u32 addr);
u16 bios_read16(u32 addr);
u32 bios_read32(u32 addr);

void bios_write8(u32 addr, u8 value);
void bios_write16(u32 addr, u16 value);
void bios_write32(u32 addr, u32 value);

#endif // BIOS_H
