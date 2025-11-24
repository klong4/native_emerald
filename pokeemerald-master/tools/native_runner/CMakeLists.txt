cmake_minimum_required(VERSION 3.10)
project(pokeemerald_native_runner C)

find_package(SDL2 REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/../../include)

add_executable(native_runner
    native_main.c
    gba_shim.c
    ai_input.c
    policy_example.c
    mlp_inference.c
    shims_extra.c
    game_stubs.c
    game_init.c
)

target_compile_definitions(native_runner PRIVATE -DNATIVE_RUNNER=1)
target_link_libraries(native_runner ${SDL2_LIBRARIES})

# Force-include the GBA compatibility header for MSVC (defines __attribute__ as no-op)
if(MSVC)
    target_compile_options(native_runner PRIVATE /FI"${CMAKE_SOURCE_DIR}/gba_compat.h")
    # Suppress C2036 (void* arithmetic) and C4616 (invalid warning pragma number)
    target_compile_options(native_runner PRIVATE /wd4616 /wd4047)
    # Allow void* pointer arithmetic as GCC extension (non-standard but common in GBA code)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Optionally include the game core sources (experimental)
option(BUILD_GAME_CORE "Include game core sources from project src/ (may require many platform shims)" OFF)
if(BUILD_GAME_CORE)
    message(STATUS "BUILD_GAME_CORE=ON: adding ../../src to include path and attempting to compile core files")
    include_directories(${CMAKE_SOURCE_DIR}/../../src)
    
    # Core game files - adding in functional groups
    set(GAME_SOURCES
        # Main entry point and game loop
        ${CMAKE_SOURCE_DIR}/../../src/main.c
        
        # Graphics and rendering
        ${CMAKE_SOURCE_DIR}/../../src/bg.c
        ${CMAKE_SOURCE_DIR}/../../src/graphics.c
        ${CMAKE_SOURCE_DIR}/../../src/gpu_regs.c
        ${CMAKE_SOURCE_DIR}/../../src/palette.c
        ${CMAKE_SOURCE_DIR}/../../src/sprite.c
        ${CMAKE_SOURCE_DIR}/../../src/window.c
        ${CMAKE_SOURCE_DIR}/../../src/decompress.c
        ${CMAKE_SOURCE_DIR}/../../src/dma3_manager.c
        
        # Input and text
        ${CMAKE_SOURCE_DIR}/../../src/text.c
        ${CMAKE_SOURCE_DIR}/../../src/text_window.c
        ${CMAKE_SOURCE_DIR}/../../src/menu.c
        ${CMAKE_SOURCE_DIR}/../../src/menu_helpers.c
        ${CMAKE_SOURCE_DIR}/../../src/string_util.c
        ${CMAKE_SOURCE_DIR}/../../src/international_string_util.c
        
        # Core utilities
        ${CMAKE_SOURCE_DIR}/../../src/malloc.c
        ${CMAKE_SOURCE_DIR}/../../src/random.c
        ${CMAKE_SOURCE_DIR}/../../src/util.c
        ${CMAKE_SOURCE_DIR}/../../src/scanline_effect.c
        ${CMAKE_SOURCE_DIR}/../../src/rtc.c
        ${CMAKE_SOURCE_DIR}/../../src/m4a.c
        ${CMAKE_SOURCE_DIR}/../../src/sound.c
        ${CMAKE_SOURCE_DIR}/../../src/play_time.c
        
        # Task system
        ${CMAKE_SOURCE_DIR}/../../src/task.c
        
        # Overworld
        ${CMAKE_SOURCE_DIR}/../../src/overworld.c
        ${CMAKE_SOURCE_DIR}/../../src/field_player_avatar.c
        ${CMAKE_SOURCE_DIR}/../../src/field_camera.c
        ${CMAKE_SOURCE_DIR}/../../src/field_effect.c
        ${CMAKE_SOURCE_DIR}/../../src/field_screen_effect.c
        ${CMAKE_SOURCE_DIR}/../../src/field_weather.c
        ${CMAKE_SOURCE_DIR}/../../src/fieldmap.c
        ${CMAKE_SOURCE_DIR}/../../src/event_object_movement.c
        ${CMAKE_SOURCE_DIR}/../../src/event_data.c
        ${CMAKE_SOURCE_DIR}/../../src/metatile_behavior.c
        ${CMAKE_SOURCE_DIR}/../../src/tileset_anims.c
        
        # Pokemon data
        ${CMAKE_SOURCE_DIR}/../../src/pokemon.c
        ${CMAKE_SOURCE_DIR}/../../src/pokemon_icon.c
        ${CMAKE_SOURCE_DIR}/../../src/pokemon_storage_system.c
        ${CMAKE_SOURCE_DIR}/../../src/data.c
        ${CMAKE_SOURCE_DIR}/../../src/pokedex.c
        ${CMAKE_SOURCE_DIR}/../../src/party_menu.c
        
        # Battle system (already had controllers)
        ${CMAKE_SOURCE_DIR}/../../src/battle_controllers.c
        ${CMAKE_SOURCE_DIR}/../../src/battle_controller_player.c
        ${CMAKE_SOURCE_DIR}/../../src/battle_controller_opponent.c
        ${CMAKE_SOURCE_DIR}/../../src/battle_main.c
        ${CMAKE_SOURCE_DIR}/../../src/battle_util.c
        ${CMAKE_SOURCE_DIR}/../../src/battle_script_commands.c
        ${CMAKE_SOURCE_DIR}/../../src/battle_message.c
        ${CMAKE_SOURCE_DIR}/../../src/battle_bg.c
        ${CMAKE_SOURCE_DIR}/../../src/battle_gfx_sfx_util.c
        ${CMAKE_SOURCE_DIR}/../../src/battle_interface.c
        ${CMAKE_SOURCE_DIR}/../../src/battle_ai_switch_items.c
        ${CMAKE_SOURCE_DIR}/../../src/battle_ai_script_commands.c
        
        # Items and shop
        ${CMAKE_SOURCE_DIR}/../../src/item.c
        ${CMAKE_SOURCE_DIR}/../../src/item_menu.c
        ${CMAKE_SOURCE_DIR}/../../src/item_use.c
        ${CMAKE_SOURCE_DIR}/../../src/shop.c
        
        # Save/load
        ${CMAKE_SOURCE_DIR}/../../src/load_save.c
        ${CMAKE_SOURCE_DIR}/../../src/new_game.c
        ${CMAKE_SOURCE_DIR}/../../src/pokemon_jump.c
        
        # Link/communication (will be stubbed)
        ${CMAKE_SOURCE_DIR}/../../src/link.c
        
        # Script engine
        ${CMAKE_SOURCE_DIR}/../../src/script.c
        
        # Trainer and NPCs
        ${CMAKE_SOURCE_DIR}/../../src/trainer_see.c
        ${CMAKE_SOURCE_DIR}/../../src/rotating_gate.c
        
        # Intro/title
        ${CMAKE_SOURCE_DIR}/../../src/intro.c
        ${CMAKE_SOURCE_DIR}/../../src/title_screen.c
    )
    
    target_sources(native_runner PRIVATE ${GAME_SOURCES})
endif()

# If a generated MLP model header exists, define MLP_MODEL_PRESENT
if(EXISTS "${CMAKE_SOURCE_DIR}/mlp_model.h")
    message(STATUS "Found mlp_model.h — enabling embedded MLP inference")
    add_compile_definitions(MLP_MODEL_PRESENT=1)
else()
    message(STATUS "No mlp_model.h found in native_runner; embedded MLP disabled")
endif()
