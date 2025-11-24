#include "rom_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool load_rom(const char *filepath, u8 **rom_data, u32 *rom_size) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open ROM file: %s\n", filepath);
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0 || file_size > ROM_SIZE) {
        fprintf(stderr, "Error: Invalid ROM size: %ld bytes\n", file_size);
        fclose(file);
        return false;
    }

    // Allocate ROM buffer
    *rom_data = (u8*)malloc(file_size);
    if (!*rom_data) {
        fprintf(stderr, "Error: Could not allocate %ld bytes for ROM\n", file_size);
        fclose(file);
        return false;
    }

    // Read ROM
    size_t read = fread(*rom_data, 1, file_size, file);
    fclose(file);

    if (read != (size_t)file_size) {
        fprintf(stderr, "Error: Could not read full ROM file\n");
        free(*rom_data);
        return false;
    }

    *rom_size = (u32)file_size;
    
    printf("ROM loaded: %s (%u bytes)\n", filepath, *rom_size);
    return true;
}

bool verify_rom_header(const u8 *rom) {
    if (!rom) return false;

    // Check Nintendo logo (bytes 0x04-0x9F)
    // For now, just check if header exists
    // Real verification would check the actual logo bytes
    
    // Check game code at 0xAC
    const char *game_code = (const char*)(rom + 0xAC);
    
    // Pokemon Emerald codes: BPEE (English), BPEJ (Japanese), etc.
    if (strncmp(game_code, "BPEE", 4) == 0 ||
        strncmp(game_code, "BPEJ", 4) == 0 ||
        strncmp(game_code, "BPEP", 4) == 0) {
        return true;
    }

    fprintf(stderr, "Warning: ROM may not be Pokemon Emerald (code: %.4s)\n", game_code);
    return true;  // Allow non-Emerald ROMs for testing
}

void parse_rom_header(const u8 *rom, ROMInfo *info) {
    if (!rom || !info) return;

    // Game title at 0xA0 (12 bytes)
    memcpy(info->game_title, rom + 0xA0, 12);
    info->game_title[12] = '\0';

    // Game code at 0xAC (4 bytes)
    memcpy(info->game_code, rom + 0xAC, 4);
    info->game_code[4] = '\0';

    // Maker code at 0xB0 (2 bytes)
    memcpy(info->maker_code, rom + 0xB0, 2);
    info->maker_code[2] = '\0';

    // Version at 0xBC (1 byte)
    info->version = rom[0xBC];

    // Header checksum at 0xBD (1 byte)
    u8 header_checksum = rom[0xBD];
    
    // Calculate checksum
    u8 calculated_checksum = 0;
    for (int i = 0xA0; i <= 0xBC; i++) {
        calculated_checksum = (u8)(calculated_checksum - rom[i]);
    }
    calculated_checksum = (u8)(calculated_checksum - 0x19);

    info->valid = (calculated_checksum == header_checksum);
    
    if (!info->valid) {
        fprintf(stderr, "Warning: ROM header checksum mismatch (expected 0x%02X, got 0x%02X)\n",
                header_checksum, calculated_checksum);
    }
}

void print_rom_info(const ROMInfo *info) {
    if (!info) return;

    printf("\n=== ROM Information ===\n");
    printf("Title:       %s\n", info->game_title);
    printf("Game Code:   %s\n", info->game_code);
    printf("Maker Code:  %s\n", info->maker_code);
    printf("Version:     %d\n", info->version);
    printf("Valid:       %s\n", info->valid ? "Yes" : "No");
    printf("======================\n\n");
}
