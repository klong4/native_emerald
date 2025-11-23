#!/bin/bash
# Test script to verify graphics and input work

echo "Starting Pokemon Emerald emulator..."
echo "The SDL window should appear with graphics."
echo ""
echo "Manual test instructions:"
echo "1. Wait for the title screen (DISPCNT should become non-zero)"
echo "2. Press ENTER (START button) to progress"
echo "3. Press Z (A button) to select menu options"
echo "4. Press arrow keys to navigate"
echo "5. Press ESC to quit"
echo ""
echo "Running emulator for 30 seconds..."
echo ""

cd /mnt/e/pokeplayer/native_emerald
DISPLAY=:0 timeout 30 build_wsl/pokemon_emu ../source/original/pkmnem.gba 2>&1 | \
    grep -E '(Frame [0-9]+|DISPCNT|Total frames|Warning)' | tail -30
