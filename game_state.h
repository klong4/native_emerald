#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "types.h"

typedef struct Memory Memory;

// Pokemon Emerald memory addresses
#define ADDR_PLAYER_NAME      0x02024029
#define ADDR_PLAYER_GENDER    0x02024008
#define ADDR_PLAYER_ID        0x0202402C
#define ADDR_GAME_TIME        0x02024030
#define ADDR_MAP_GROUP        0x02036DFC
#define ADDR_MAP_NUMBER       0x02036DFD
#define ADDR_PLAYER_X         0x02037340
#define ADDR_PLAYER_Y         0x02037344
#define ADDR_BADGES           0x0202420C
#define ADDR_MONEY            0x02024490
#define ADDR_COINS            0x02024494
#define ADDR_PARTY_COUNT      0x02024284
#define ADDR_PARTY_DATA       0x02024284

// Pokemon structure offsets
#define POKEMON_SIZE          100
#define POKEMON_SPECIES       0x00
#define POKEMON_HP            0x56
#define POKEMON_MAX_HP        0x58
#define POKEMON_ATTACK        0x5A
#define POKEMON_DEFENSE       0x5C
#define POKEMON_SPEED         0x5E
#define POKEMON_SP_ATTACK     0x60
#define POKEMON_SP_DEFENSE    0x62
#define POKEMON_LEVEL         0x54
#define POKEMON_EXP           0x50
#define POKEMON_STATUS        0x50

typedef struct {
    u16 species;
    u16 hp;
    u16 max_hp;
    u16 attack;
    u16 defense;
    u16 speed;
    u16 sp_attack;
    u16 sp_defense;
    u8 level;
    u32 exp;
    u32 status;
} PokemonData;

typedef struct {
    char player_name[8];
    u8 player_gender;
    u32 player_id;
    u32 game_time_hours;
    u32 game_time_minutes;
    u8 map_group;
    u8 map_number;
    u16 player_x;
    u16 player_y;
    u8 badges;
    u32 money;
    u16 coins;
    u8 party_count;
    PokemonData party[6];
} GameState;

// Extract game state from memory
bool extract_game_state(Memory *mem, GameState *state);

// Get specific values
u8 get_badge_count(Memory *mem);
u32 get_player_money(Memory *mem);
u16 get_party_total_hp(Memory *mem);
u16 get_party_total_max_hp(Memory *mem);
bool is_in_battle(Memory *mem);
u8 get_current_map(Memory *mem);

#endif // GAME_STATE_H
