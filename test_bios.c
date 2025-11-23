#include <stdio.h>
#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;

static u8 bios_memory[0x4000];

int main() {
    u32 vectors_v2[] = {
        0xEA000001,  // 0x00
        0xEA000000,  // 0x04
        0xEA000001,  // 0x08
        0xEA000000,  // 0x0C
        0xEA000000,  // 0x10
        0xE59FF004,  // 0x14
        0xEA000004,  // 0x18
        0xEA000000,  // 0x1C
        0x08000000,  // 0x20
    };
    
    for (int i = 0; i < 9; i++) {
        bios_memory[i*4 + 0] = (vectors_v2[i] >> 0) & 0xFF;
        bios_memory[i*4 + 1] = (vectors_v2[i] >> 8) & 0xFF;
        bios_memory[i*4 + 2] = (vectors_v2[i] >> 16) & 0xFF;
        bios_memory[i*4 + 3] = (vectors_v2[i] >> 24) & 0xFF;
    }
    
    printf(" BIOS memory at 0x14:\\n\);
