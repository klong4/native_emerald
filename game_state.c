#include "game_state.h"
#include "memory.h"
#include <string.h>

bool extract_game_state(Memory *mem, GameState *state) {
    if (!mem || !state) return false;
    
    memset(state, 0, sizeof(GameState));
    
    // Player name (7 bytes + null terminator)
    for (int i = 0; i < 7; i++) {
        state->player_name[i] = mem_read8(mem, ADDR_PLAYER_NAME + i);
    }
    state->player_name[7] = '\0';
    
    // Player info
    state->player_gender = mem_read8(mem, ADDR_PLAYER_GENDER);
    state->player_id = mem_read32(mem, ADDR_PLAYER_ID);
    
    // Game time
    state->game_time_hours = mem_read16(mem, ADDR_GAME_TIME);
    state->game_time_minutes = mem_read8(mem, ADDR_GAME_TIME + 2);
    
    // Location
    state->map_group = mem_read8(mem, ADDR_MAP_GROUP);
    state->map_number = mem_read8(mem, ADDR_MAP_NUMBER);
    state->player_x = mem_read16(mem, ADDR_PLAYER_X);
    state->player_y = mem_read16(mem, ADDR_PLAYER_Y);
    
    // Progress
    state->badges = mem_read8(mem, ADDR_BADGES);
    state->money = mem_read32(mem, ADDR_MONEY);
    state->coins = mem_read16(mem, ADDR_COINS);
    
    // Party
    state->party_count = mem_read8(mem, ADDR_PARTY_COUNT);
    if (state->party_count > 6) state->party_count = 6;
    
    for (u8 i = 0; i < state->party_count; i++) {
        u32 pokemon_addr = ADDR_PARTY_DATA + 4 + (i * POKEMON_SIZE);
        
        state->party[i].species = mem_read16(mem, pokemon_addr + POKEMON_SPECIES);
        state->party[i].hp = mem_read16(mem, pokemon_addr + POKEMON_HP);
        state->party[i].max_hp = mem_read16(mem, pokemon_addr + POKEMON_MAX_HP);
        state->party[i].attack = mem_read16(mem, pokemon_addr + POKEMON_ATTACK);
        state->party[i].defense = mem_read16(mem, pokemon_addr + POKEMON_DEFENSE);
        state->party[i].speed = mem_read16(mem, pokemon_addr + POKEMON_SPEED);
        state->party[i].sp_attack = mem_read16(mem, pokemon_addr + POKEMON_SP_ATTACK);
        state->party[i].sp_defense = mem_read16(mem, pokemon_addr + POKEMON_SP_DEFENSE);
        state->party[i].level = mem_read8(mem, pokemon_addr + POKEMON_LEVEL);
        state->party[i].exp = mem_read32(mem, pokemon_addr + POKEMON_EXP);
    }
    
    return true;
}

u8 get_badge_count(Memory *mem) {
    u8 badges = mem_read8(mem, ADDR_BADGES);
    u8 count = 0;
    for (int i = 0; i < 8; i++) {
        if (badges & (1 << i)) count++;
    }
    return count;
}

u32 get_player_money(Memory *mem) {
    return mem_read32(mem, ADDR_MONEY);
}

u16 get_party_total_hp(Memory *mem) {
    u8 party_count = mem_read8(mem, ADDR_PARTY_COUNT);
    if (party_count > 6) party_count = 6;
    
    u16 total = 0;
    for (u8 i = 0; i < party_count; i++) {
        u32 pokemon_addr = ADDR_PARTY_DATA + 4 + (i * POKEMON_SIZE);
        total += mem_read16(mem, pokemon_addr + POKEMON_HP);
    }
    return total;
}

u16 get_party_total_max_hp(Memory *mem) {
    u8 party_count = mem_read8(mem, ADDR_PARTY_COUNT);
    if (party_count > 6) party_count = 6;
    
    u16 total = 0;
    for (u8 i = 0; i < party_count; i++) {
        u32 pokemon_addr = ADDR_PARTY_DATA + 4 + (i * POKEMON_SIZE);
        total += mem_read16(mem, pokemon_addr + POKEMON_MAX_HP);
    }
    return total;
}

bool is_in_battle(Memory *mem) {
    // Check battle flags (simplified - actual implementation would be more complex)
    u8 battle_flags = mem_read8(mem, 0x02022B4C);
    return battle_flags != 0;
}

u8 get_current_map(Memory *mem) {
    return mem_read8(mem, ADDR_MAP_NUMBER);
}
