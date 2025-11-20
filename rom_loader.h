#ifndef ROM_LOADER_H
#define ROM_LOADER_H

#include "types.h"

typedef struct {
    char game_title[13];
    char game_code[5];
    char maker_code[3];
    u8 version;
    u32 rom_size;
    bool valid;
} ROMInfo;

bool load_rom(const char *filepath, u8 **rom_data, u32 *rom_size);
bool verify_rom_header(const u8 *rom);
void parse_rom_header(const u8 *rom, ROMInfo *info);
void print_rom_info(const ROMInfo *info);

#endif // ROM_LOADER_H
