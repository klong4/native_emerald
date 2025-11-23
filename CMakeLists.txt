cmake_minimum_required(VERSION 3.10)
project(pokemon_emerald_emulator C)

set(CMAKE_C_STANDARD 11)

# Try multiple methods to find SDL2
# Method 1: vcpkg (if VCPKG_ROOT env var is set)
if(DEFINED ENV{VCPKG_ROOT})
    message(STATUS "Found VCPKG_ROOT: $ENV{VCPKG_ROOT}")
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif()

# Method 2: Try CONFIG mode (vcpkg)
find_package(SDL2 CONFIG QUIET)

# Method 3: Try MODULE mode with pkg-config (Linux)
if(NOT SDL2_FOUND)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2 QUIET sdl2)
    endif()
endif()

# Method 4: Manual search (Windows with manual SDL2 install)
if(NOT SDL2_FOUND)
    message(STATUS "Trying manual SDL2 search...")
    find_path(SDL2_INCLUDE_DIR SDL.h
        HINTS
        C:/SDL2/include
        C:/SDL2-2.*/include
        ${SDL2_DIR}/include
        ENV SDL2_DIR
    )
    find_library(SDL2_LIBRARY
        NAMES SDL2
        HINTS
        C:/SDL2/lib/x64
        C:/SDL2-2.*/lib/x64
        ${SDL2_DIR}/lib/x64
        ENV SDL2_DIR
    )
    if(SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
        set(SDL2_FOUND TRUE)
        set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
        set(SDL2_LIBRARIES ${SDL2_LIBRARY})
        message(STATUS "Found SDL2: ${SDL2_LIBRARY}")
    endif()
endif()

# Final check
if(NOT SDL2_FOUND)
    message(FATAL_ERROR 
        "SDL2 not found! Please install SDL2:\n\n"
        "  Windows: Download from https://www.libsdl.org/download-2.0.php\n"
        "           Extract to C:\\SDL2 and retry\n\n"
        "  WSL/Linux: sudo apt install libsdl2-dev\n\n"
        "  Or set SDL2_DIR to your SDL2 installation:\n"
        "    cmake .. -DSDL2_DIR=/path/to/SDL2\n")
endif()

message(STATUS "SDL2 found!")

# Source files
set(SOURCES
    main.c
    rom_loader.c
    memory.c
    cpu_core.c
    gfx_renderer.c
    input.c
    stubs.c
    interrupts.c
    python_api.c
    bios.c
    debug_trace.c
    timer.c
    dma.c
    rtc.c
)

set(HEADERS
    rom_loader.h
    memory.h
    cpu_core.h
    gfx_renderer.h
    input.h
    stubs.h
    types.h
    interrupts.h
    python_api.h
    bios.h
    debug_trace.h
    timer.h
    dma.h
    rtc.h
)

# Future additions (require refactoring):
# save_state.c save_state.h
# game_state.c game_state.h

# Main executable
add_executable(pokemon_emu ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(pokemon_emu PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SDL2_INCLUDE_DIRS}
)

# Link SDL2 (handle both vcpkg CONFIG and manual modes)
if(TARGET SDL2::SDL2)
    # vcpkg CONFIG mode
    target_link_libraries(pokemon_emu PRIVATE SDL2::SDL2 SDL2::SDL2main)
else()
    # Manual or pkg-config mode
    target_link_libraries(pokemon_emu PRIVATE ${SDL2_LIBRARIES})
    if(WIN32)
        # Windows needs SDLmain for WinMain entry point
        find_library(SDL2MAIN_LIBRARY
            NAMES SDL2main
            HINTS
            C:/SDL2/lib/x64
            C:/SDL2-2.*/lib/x64
            ${SDL2_DIR}/lib/x64
        )
        if(SDL2MAIN_LIBRARY)
            target_link_libraries(pokemon_emu PRIVATE ${SDL2MAIN_LIBRARY})
        endif()
    endif()
endif()

if(MSVC)
    target_compile_options(pokemon_emu PRIVATE /W4)
    target_compile_definitions(pokemon_emu PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(pokemon_emu PRIVATE -Wall -Wextra -O3)
endif()

# Optional: Build as shared library for Python ctypes
option(BUILD_PYTHON_LIB "Build shared library for Python" ON)
if(BUILD_PYTHON_LIB)
    add_library(pokemon_emu_lib SHARED ${SOURCES} ${HEADERS})
    
    target_include_directories(pokemon_emu_lib PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${SDL2_INCLUDE_DIRS}
    )
    
    if(TARGET SDL2::SDL2)
        target_link_libraries(pokemon_emu_lib PRIVATE SDL2::SDL2)
    else()
        target_link_libraries(pokemon_emu_lib PRIVATE ${SDL2_LIBRARIES})
    endif()
    
    target_compile_definitions(pokemon_emu_lib PRIVATE BUILD_PYTHON_LIB=1)
    
    if(MSVC)
        # Export all symbols for ctypes
        set_target_properties(pokemon_emu_lib PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
        target_compile_definitions(pokemon_emu_lib PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

# Tests (optional)
option(BUILD_TESTS "Build test suite" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS pokemon_emu DESTINATION bin)
if(BUILD_PYTHON_LIB)
    install(TARGETS pokemon_emu_lib DESTINATION lib)
endif()
