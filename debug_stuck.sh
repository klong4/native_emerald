#!/bin/bash
# Debug script to check what the game is doing when stuck

# Get PC value from emulator output
PC=0x08001002

# Calculate ROM offset (PC - 0x08000000)
OFFSET=$((PC - 0x08000000))

# Show bytes at that location
echo "Checking ROM at PC=$PC (offset=0x$(printf '%X' $OFFSET))"
xxd -s $OFFSET -l 16 /mnt/e/pokeplayer/native_emerald/pokeemerald.gba

# Disassemble (if we had arm-none-eabi-objdump)
# This would show: "b 0x08001002"  (infinite loop: branch to self)
