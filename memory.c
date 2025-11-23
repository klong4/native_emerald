#include "memory.h"
#include "interrupts.h"
#include "bios.h"
#include "timer.h"
#include "dma.h"
#include "rtc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int warning_count = 0;
static const int MAX_WARNINGS = 10;

void mem_init(Memory *mem) {
    if (!mem) return;
    
    mem->rom = NULL;
    mem->rom_size = 0;
    mem->interrupts = NULL;
    mem->rtc = NULL;
    
    memset(mem->ewram, 0, EWRAM_SIZE);
    memset(mem->iwram, 0, IWRAM_SIZE);
    memset(mem->vram, 0, VRAM_SIZE);
    memset(mem->oam, 0, OAM_SIZE);
    memset(mem->palette, 0, PALETTE_SIZE);
    memset(mem->io_regs, 0, IO_SIZE);
    memset(mem->sram, 0xFF, sizeof(mem->sram)); // Flash memory defaults to 0xFF
    
    // Initialize I/O registers to proper power-on values (from mGBA)
    // DISPCNT = 0x0080 (forced blank)
    mem->io_regs[0x00] = 0x80;
    mem->io_regs[0x01] = 0x00;
    
    // KEYINPUT = 0x03FF (all buttons released, active-low)
    mem->io_regs[0x130] = 0xFF;
    mem->io_regs[0x131] = 0x03;
    
    // SOUNDBIAS = 0x0200
    mem->io_regs[0x88] = 0x00;
    mem->io_regs[0x89] = 0x02;
    
    // BG2PA = 0x0100 (affine parameter A for BG2)
    mem->io_regs[0x20] = 0x00;
    mem->io_regs[0x21] = 0x01;
    
    // BG2PD = 0x0100 (affine parameter D for BG2)
    mem->io_regs[0x26] = 0x00;
    mem->io_regs[0x27] = 0x01;
    
    // BG3PA = 0x0100 (affine parameter A for BG3)
    mem->io_regs[0x30] = 0x00;
    mem->io_regs[0x31] = 0x01;
    
    // BG3PD = 0x0100 (affine parameter D for BG3)
    mem->io_regs[0x36] = 0x00;
    mem->io_regs[0x37] = 0x01;
    
    // When running without BIOS (HLE), set POST flag
    // POSTFLG (0x0410_0300 mapped to I/O 0x300) = 1
    // Actually POSTFLG is at 0x04000300 but it's beyond normal I/O range
    // For now, we'll handle it in the special case in mem_write8
    
    // VCOUNT should start at a reasonable scanline when no BIOS
    // Set to 0x7E (126) as mGBA does
    mem->io_regs[0x06] = 0x7E;
    mem->io_regs[0x07] = 0x00;
    
    // Initialize GPIO registers for RTC support
    mem->gpio_data = 0;
    mem->gpio_direction = 0;
    mem->gpio_control = 1;  // Bit 0 = 1 enables GPIO (required for RTC detection)
    
    // Initialize Flash chip emulation
    mem->flash_state = 0;
    mem->flash_cmd = 0;
    
    // Initialize BIOS
    bios_init();
}

void mem_cleanup(Memory *mem) {
    // ROM is owned by caller, don't free it here
    mem->rom = NULL;
    mem->rom_size = 0;
}

void mem_set_rom(Memory *mem, u8 *rom, u32 size) {
    mem->rom = rom;
    mem->rom_size = size;
}

void mem_set_interrupts(Memory *mem, InterruptState *interrupts) {
    mem->interrupts = interrupts;
}

void mem_set_timers(Memory *mem, TimerState *timers) {
    mem->timers = timers;
}

void mem_set_dma(Memory *mem, DMAState *dma) {
    mem->dma = dma;
}

void mem_set_rtc(Memory *mem, RTCState *rtc) {
    mem->rtc = rtc;
}

u8 mem_read8(Memory *mem, u32 addr) {
    // EWRAM: 0x02000000 - 0x02FFFFFF (mirrored 256KB)
    // The GBA mirrors EWRAM throughout the 16MB region
    if (addr >= 0x02000000 && addr < 0x03000000) {
        u32 offset = (addr - 0x02000000) % EWRAM_SIZE;
        return mem->ewram[offset];
    }
    
    // IWRAM: 0x03000000 - 0x03FFFFFF (32KB mirrored throughout 16MB region)
    if (addr >= ADDR_IWRAM_START && addr < 0x04000000) {
        u32 offset = (addr - ADDR_IWRAM_START) % IWRAM_SIZE;
        return mem->iwram[offset];
    }
    
    // IWRAM mirror: 0x01000000 - 0x01FFFFFF (used by BIOS/stack)
    // GBA mirrors IWRAM throughout this region
    if (addr >= 0x01000000 && addr < 0x02000000) {
        u32 offset = (addr - 0x01000000) % IWRAM_SIZE;
        return mem->iwram[offset];
    }
    
    // I/O Registers: 0x04000000 - 0x040003FF (1KB)
    if (addr >= ADDR_IO_START && addr < ADDR_IO_START + IO_SIZE) {
        u32 offset = addr - ADDR_IO_START;
        
        // VCOUNT register (0x06-0x07) - returns current scanline from interrupt state
        if (offset == 0x06 || offset == 0x07) {
            if (mem->interrupts) {
                return (offset == 0x06) ? (mem->interrupts->vcount & 0xFF) : ((mem->interrupts->vcount >> 8) & 0xFF);
            }
            return 0;
        }
        
        // Return live interrupt register values
        if (mem->interrupts) {
            if (offset == REG_IE || offset == REG_IE + 1) {
                return (offset == REG_IE) ? (mem->interrupts->ie & 0xFF) : ((mem->interrupts->ie >> 8) & 0xFF);
            }
            if (offset == REG_IF || offset == REG_IF + 1) {
                return (offset == REG_IF) ? (mem->interrupts->if_flag & 0xFF) : ((mem->interrupts->if_flag >> 8) & 0xFF);
            }
            if (offset == REG_IME || offset == REG_IME + 1) {
                return (offset == REG_IME) ? (mem->interrupts->ime & 0xFF) : ((mem->interrupts->ime >> 8) & 0xFF);
            }
            if (offset == 0x04 || offset == 0x05) {
                return (offset == 0x04) ? (mem->interrupts->dispstat & 0xFF) : ((mem->interrupts->dispstat >> 8) & 0xFF);
            }
        }
        
        // Sound registers: 0x60-0xA7 (Sound control, channels, Wave RAM)
        if (offset >= 0x60 && offset <= 0xA7) {
            // Return stored values or 0 for unimplemented sound
            return mem->io_regs[offset];
        }
        
        // Undocumented/unused registers (stub to prevent warnings)
        if (offset >= 0xE00 && offset <= 0xFFFF) {
            // Unknown register area, return 0
            return 0;
        }
        
        // Keypad registers: 0x130-0x133
        if (offset >= 0x130 && offset <= 0x133) {
            // KEYINPUT (0x130) and KEYCNT (0x132) are read from io_regs
            u8 value = mem->io_regs[offset];
            
            // Debug: Log KEYINPUT reads to verify input is working
            static int keyinput_log_count = 0;
            if (offset == 0x130 && keyinput_log_count < 20) {
                u16 keyinput = mem->io_regs[0x130] | (mem->io_regs[0x131] << 8);
                printf("[INPUT] KEYINPUT read = 0x%04X (A=%d B=%d Start=%d Select=%d)\n",
                    keyinput,
                    (keyinput & 0x01) ? 0 : 1,  // Active low
                    (keyinput & 0x02) ? 0 : 1,
                    (keyinput & 0x08) ? 0 : 1,
                    (keyinput & 0x04) ? 0 : 1);
                keyinput_log_count++;
            }
            
            return value;
        }
        
        // Timer registers: 0x100-0x10F (4 timers, 4 bytes each)
        if (mem->timers && offset >= 0x100 && offset <= 0x10F) {
            int timer_id = (offset - 0x100) / 4;
            int reg = (offset - 0x100) % 4;
            
            if (reg == 0 || reg == 1) {
                // Counter value (bytes 0-1) - read live value
                u16 counter = timer_read_counter(mem->timers, timer_id);
                return (reg == 0) ? (counter & 0xFF) : ((counter >> 8) & 0xFF);
            } else {
                // Control register (bytes 2-3) - read from io_regs
                return mem->io_regs[offset];
            }
        }
        
        // Log first read from unknown I/O registers to help debug
        static u32 io_read_log[512] = {0};
        if (offset < 512 && io_read_log[offset] == 0) {
            printf("[I/O] First read from 0x04%06X\n", offset);
            io_read_log[offset] = 1;
        }
        
        return mem->io_regs[offset];
    }
    
    // Palette RAM: 0x05000000 - 0x050003FF (1KB)
    if (addr >= ADDR_PALETTE_START && addr < ADDR_PALETTE_START + PALETTE_SIZE) {
        return mem->palette[addr - ADDR_PALETTE_START];
    }
    
    // VRAM: 0x06000000 - 0x06017FFF (96KB, mirrors every 128KB up to 0x07000000)
    if (addr >= ADDR_VRAM_START && addr < ADDR_OAM_START) {
        u32 offset = (addr - ADDR_VRAM_START) % (128 * 1024); // 128KB mirror
        if (offset < VRAM_SIZE) {
            return mem->vram[offset];
        }
        // Out of bounds within mirror returns 0
        return 0;
    }
    
    // OAM: 0x07000000 - 0x070003FF (1KB)
    if (addr >= ADDR_OAM_START && addr < ADDR_OAM_START + OAM_SIZE) {
        return mem->oam[addr - ADDR_OAM_START];
    }
    
    // ROM: 0x08000000 - 0x09FFFFFF (max 32MB)
    // Also mirrored at 0x0A000000 and 0x0C000000
    if (addr >= ADDR_ROM_START && addr < ADDR_ROM_START + ROM_SIZE) {
        // GPIO registers for RTC (Pokemon Emerald uses these)
        if (addr == 0x080000C4) {
            // GPIO data read - incorporate RTC data bit
            u8 base_value = mem->gpio_data & 0xFF;
            if (mem->rtc) {
                u8 rtc_bit = rtc_gpio_read(mem->rtc, mem->gpio_data, mem->gpio_direction);
                // Merge RTC bit into GPIO data
                base_value = (base_value & ~0x02) | (rtc_bit & 0x02);
            }
            
            static int gpio_read_count = 0;
            if (gpio_read_count < 10) {
                printf("[GPIO] Read from 0x080000C4 (GPIO_DATA) = 0x%02X\n", base_value);
                gpio_read_count++;
            }
            return base_value;
        }
        if (addr == 0x080000C5) {
            static int gpio_read_h_count = 0;
            if (gpio_read_h_count < 5) {
                printf("[GPIO] Read from 0x080000C5 (GPIO_DATA high) = 0x%02X\n", (mem->gpio_data >> 8) & 0xFF);
                gpio_read_h_count++;
            }
            return (mem->gpio_data >> 8) & 0xFF;
        }
        if (addr == 0x080000C6) {
            static int gpio_dir_count = 0;
            if (gpio_dir_count < 5) {
                printf("[GPIO] Read from 0x080000C6 (GPIO_DIRECTION) = 0x%02X\n", mem->gpio_direction & 0xFF);
                gpio_dir_count++;
            }
            return mem->gpio_direction & 0xFF;
        }
        if (addr == 0x080000C7) return (mem->gpio_direction >> 8) & 0xFF;
        if (addr == 0x080000C8) {
            static int gpio_ctrl_count = 0;
            if (gpio_ctrl_count < 5) {
                printf("[GPIO] Read from 0x080000C8 (GPIO_CONTROL) = 0x%02X (bit0=GPIO enable)\n", mem->gpio_control & 0xFF);
                gpio_ctrl_count++;
            }
            return mem->gpio_control & 0xFF;
        }
        if (addr == 0x080000C9) return (mem->gpio_control >> 8) & 0xFF;
        
        u32 offset = (addr - ADDR_ROM_START) % mem->rom_size;
        return mem->rom[offset];
    }
    
    // Unmapped region 0x07000400 - 0x07FFFFFF returns open bus (last value or 0)
    // This is between OAM end and ROM start
    if (addr >= 0x07000400 && addr < 0x08000000) {
        // Return 0 for unmapped reads (open bus behavior)
        return 0;
    }
    
    // SRAM/Flash: 0x0E000000 - 0x0E00FFFF (64KB) or 0x0E01FFFF (128KB)
    // Pokemon Emerald uses 128KB Flash (Macronix MX29L1011)
    if (addr >= 0x0E000000 && addr < 0x0E020000) {
        u32 offset = addr - 0x0E000000;
        
        // Flash chip identification mode
        if (mem->flash_state == 1 && mem->flash_cmd == 0x90) {
            if (offset == 0x0000) return 0xC2; // Manufacturer ID (Macronix)
            if (offset == 0x0001) return 0x09; // Device ID (MX29L1011 - 128KB)
            // Don't reset state immediately - let exit command (0xF0) handle it
        }
        
        if (offset < sizeof(mem->sram)) {
            return mem->sram[offset];
        }
    }
    
    // BIOS: 0x00000000 - 0x00003FFF
    // Use BIOS emulation
    if (addr < 0x00004000) {
        return bios_read8(addr);
    }
    
    // Region 0x00004000+ can be I/O mirrored from lower addresses
    // Try masking to proper regions
    if (addr >= 0x00004000 && addr < 0x01000000) {
        // Recursively read with proper address masking
        return mem_read8(mem, addr | 0x04000000);
    }
    
    // Undocumented I/O registers beyond 0x04000400 (e.g., 0x0400E04C)
    if (addr >= 0x04000400 && addr < 0x05000000) {
        // WAITCNT (0x04000204) - Wait State Control
        if (addr == 0x04000204 || addr == 0x04000205) {
            // Return default fast wait states: 0x4317
            // This configures SRAM and ROM access timing
            return (addr == 0x04000204) ? 0x17 : 0x43;
        }
        
        // POSTFLG (0x04000300) - POST boot flag
        // When set to 1, indicates BIOS has completed
        if (addr == 0x04000300) {
            return 1; // BIOS completed (HLE mode)
        }
        
        // HALTCNT (0x04000301) - Halt/Stop control
        if (addr == 0x04000301) {
            return 0; // Read-only, returns 0
        }
        
        // Unknown I/O register area, return 0
        return 0;
    }
    
    // Save type detection probes (0x09000000-0x0DFFFFFF, 0x10000000+)
    // Games read from these addresses to detect save memory type
    // Return 0xFF to indicate no memory present
    if ((addr >= 0x09000000 && addr < 0x0E000000) || addr >= 0x10000000) {
        return 0xFF; // Indicate no memory at this address
    }
    
    // Unmapped memory
    if (warning_count < MAX_WARNINGS) {
        // Try to get some context about where this access is from
        // This helps debug what instruction is causing the bad access
        fprintf(stderr, "Warning: Read from unmapped address 0x%08X (returning 0)\n", addr);
        fflush(stderr);  // Force immediate output for debugging
        warning_count++;
        if (warning_count == MAX_WARNINGS) {
            fprintf(stderr, "(Suppressing further memory warnings...)\n");
        }
    }
    return 0;
}

u16 mem_read16(Memory *mem, u32 addr) {
    // GBA is little-endian
    u8 low = mem_read8(mem, addr);
    u8 high = mem_read8(mem, addr + 1);
    return (u16)(low | (high << 8));
}

u32 mem_read32(Memory *mem, u32 addr) {
    u16 low = mem_read16(mem, addr);
    u16 high = mem_read16(mem, addr + 2);
    return (u32)(low | (high << 16));
}

void mem_write8(Memory *mem, u32 addr, u8 value) {
    // BIOS area and mirrors: 0x00000000 - 0x00FFFFFF (mirrored 16KB)
    // Allow writes to BIOS flags region
    if (addr < 0x01000000) {
        u32 offset = addr % 0x4000;  // 16KB BIOS mirrored
        bios_write8(offset, value);
        return;
    }
    
    // EWRAM: 0x02000000 - 0x02FFFFFF (mirrored 256KB)
    // The GBA mirrors EWRAM throughout the 16MB region
    if (addr >= 0x02000000 && addr < 0x03000000) {
        u32 offset = (addr - 0x02000000) % EWRAM_SIZE;
        mem->ewram[offset] = value;
        return;
    }
    
    // IWRAM: 0x03000000 - 0x03FFFFFF (32KB mirrored throughout 16MB region)
    if (addr >= ADDR_IWRAM_START && addr < 0x04000000) {
        u32 offset = (addr - ADDR_IWRAM_START) % IWRAM_SIZE;
        
        // IRQ vector logging disabled to reduce noise
        /*
        if ((addr & 0x00007FFF) >= 0x7FFC) {
            u32 real_addr = ADDR_IWRAM_START + offset;
            if (real_addr >= 0x03007FFC && real_addr <= 0x03007FFF) {
                printf("[IRQ_VEC] Write to 0x%08X (mirrored from 0x%08X) = 0x%02X\n", addr, real_addr, value);
                if ((real_addr & 0x3) == 0x3) {
                    // After last byte, show full 32-bit value
                    u32 full_val = mem_read32(mem, 0x03007FFC);
                    printf("[IRQ_VEC] Complete handler pointer = 0x%08X\n", full_val);
                }
            }
        }
        */
        mem->iwram[offset] = value;
        return;
    }
    
    // IWRAM mirror: 0x01000000 - 0x01FFFFFF (used by BIOS/stack)
    // GBA mirrors IWRAM throughout this region
    if (addr >= 0x01000000 && addr < 0x02000000) {
        u32 offset = (addr - 0x01000000) % IWRAM_SIZE;
        mem->iwram[offset] = value;
        return;
    }
    
    // I/O Registers: 0x04000000 - 0x040003FF
    if (addr >= ADDR_IO_START && addr < ADDR_IO_START + IO_SIZE) {
        u32 offset = addr - ADDR_IO_START;
        
        // DISPCNT register (0x00-0x01) - Display Control
        if (offset == 0x00 || offset == 0x01) {
            static u16 last_dispcnt = 0;
            static bool first_write = true;
            u16 dispcnt = mem_read16(mem, ADDR_IO_START);
            if (offset == 0x00) {
                dispcnt = (dispcnt & 0xFF00) | value;
            } else {
                dispcnt = (dispcnt & 0x00FF) | (value << 8);
            }
            
            // Log ALL DISPCNT writes to help debug graphics initialization
            if (dispcnt != last_dispcnt || first_write) {
                printf("\n*** [DISPLAY INIT] DISPCNT Write: 0x%04X (Mode=%d, BG0=%d, BG1=%d, BG2=%d, BG3=%d, OBJ=%d) ***\n\n",
                    dispcnt,
                    dispcnt & 0x7,
                    (dispcnt & 0x0100) ? 1 : 0,
                    (dispcnt & 0x0200) ? 1 : 0,
                    (dispcnt & 0x0400) ? 1 : 0,
                    (dispcnt & 0x0800) ? 1 : 0,
                    (dispcnt & 0x1000) ? 1 : 0);
                last_dispcnt = dispcnt;
                first_write = false;
            }
            
            mem->io_regs[0] = dispcnt & 0xFF;
            mem->io_regs[1] = (dispcnt >> 8) & 0xFF;
            return;
        }
        
        // DISPSTAT register (0x04-0x05) - Display Status
        // Log DISPSTAT writes to see if game enables interrupts
        if (offset == 0x04 || offset == 0x05) {
            static u16 last_dispstat_log = 0xFFFF;
            u16 dispstat_new = mem_read16(mem, ADDR_IO_START + 0x04);
            if (offset == 0x04) {
                dispstat_new = (dispstat_new & 0xFF00) | value;
            } else {
                dispstat_new = (dispstat_new & 0x00FF) | (value << 8);
            }
            
            if (dispstat_new != last_dispstat_log) {
                printf("[DISPSTAT] Write: 0x%04X (VBlank_IRQ=%d, HBlank_IRQ=%d, VCount_IRQ=%d, VCount_Setting=%d)\n",
                    dispstat_new,
                    (dispstat_new & 0x08) ? 1 : 0,
                    (dispstat_new & 0x10) ? 1 : 0,
                    (dispstat_new & 0x20) ? 1 : 0,
                    (dispstat_new >> 8) & 0xFF);
                last_dispstat_log = dispstat_new;
            }
            
            if (mem->interrupts) {
                mem->interrupts->dispstat = dispstat_new;
                mem->io_regs[0x04] = dispstat_new & 0xFF;
                mem->io_regs[0x05] = (dispstat_new >> 8) & 0xFF;
            }
            return;
        }
        
        // VCOUNT is read-only, ignore writes
        if (offset == 0x06 || offset == 0x07) {
            return;
        }
        
        // Handle interrupt registers specially
        if (mem->interrupts) {
            if (offset == REG_IE || offset == REG_IE + 1) {
                // IE register write
                u16 ie = mem_read16(mem, ADDR_IO_START + REG_IE);
                if (offset == REG_IE) {
                    ie = (ie & 0xFF00) | value;
                } else {
                    ie = (ie & 0x00FF) | (value << 8);
                }
                
                // Debug: Log IE changes
                static u16 last_ie = 0;
            if (ie != last_ie && false) {
                }
                
                mem->interrupts->ie = ie;
                mem->io_regs[REG_IE] = ie & 0xFF;
                mem->io_regs[REG_IE + 1] = (ie >> 8) & 0xFF;
                return;
            }
            if (offset == REG_IF || offset == REG_IF + 1) {
                // IF register write (acknowledge interrupts)
                u16 if_val = mem_read16(mem, ADDR_IO_START + REG_IF);
                
                // Debug: Log ALL IF acknowledgements to find the issue
                // printf("[IF_ACK] offset=0x%X value=0x%02X (IF before=0x%04X) ", offset, value, if_val);
                
                if (offset == REG_IF) {
                    interrupt_acknowledge(mem->interrupts, value);
                } else {
                    interrupt_acknowledge(mem->interrupts, value << 8);
                }
                mem->io_regs[REG_IF] = mem->interrupts->if_flag & 0xFF;
                mem->io_regs[REG_IF + 1] = (mem->interrupts->if_flag >> 8) & 0xFF;
                
                // printf("(IF after=0x%04X)\n", mem->interrupts->if_flag);
                return;
            }
            if (offset == REG_IME || offset == REG_IME + 1) {
                // IME register write
                u16 ime = mem_read16(mem, ADDR_IO_START + REG_IME);
                if (offset == REG_IME) {
                    ime = (ime & 0xFF00) | value;
                } else {
                    ime = (ime & 0x00FF) | (value << 8);
                }
                mem->interrupts->ime = ime;
                mem->io_regs[REG_IME] = ime & 0xFF;
                mem->io_regs[REG_IME + 1] = (ime >> 8) & 0xFF;
                return;
            }
        }
        
        // Timer registers: 0x100-0x10F (4 timers, 4 bytes each)
        if (mem->timers && offset >= 0x100 && offset <= 0x10F) {
            int timer_id = (offset - 0x100) / 4;
            int reg = (offset - 0x100) % 4;
            
            if (reg == 0 || reg == 1) {
                // Reload value (bytes 0-1)
                u16 reload = mem_read16(mem, ADDR_IO_START + 0x100 + timer_id * 4);
                if (reg == 0) {
                    reload = (reload & 0xFF00) | value;
                } else {
                    reload = (reload & 0x00FF) | (value << 8);
                }
                timer_write_reload(mem->timers, timer_id, reload);
                mem->io_regs[0x100 + timer_id * 4] = reload & 0xFF;
                mem->io_regs[0x100 + timer_id * 4 + 1] = (reload >> 8) & 0xFF;
            } else if (reg == 2 || reg == 3) {
                // Control register (bytes 2-3)
                u16 control = mem_read16(mem, ADDR_IO_START + 0x100 + timer_id * 4 + 2);
                if (reg == 2) {
                    control = (control & 0xFF00) | value;
                } else {
                    control = (control & 0x00FF) | (value << 8);
                }
                timer_write_control(mem->timers, timer_id, control);
                mem->io_regs[0x100 + timer_id * 4 + 2] = control & 0xFF;
                mem->io_regs[0x100 + timer_id * 4 + 3] = (control >> 8) & 0xFF;
            }
            return;
        }
        
        // DMA registers: 0xB0-0xDF (4 channels, 12 bytes each)
        if (mem->dma && offset >= 0xB0 && offset <= 0xDF) {
            int channel = (offset - 0xB0) / 12;
            int reg = (offset - 0xB0) % 12;
            
            DMAChannel *dma = &mem->dma->channels[channel];
            
            // Store value in IO register array
            mem->io_regs[offset] = value;
            
            if (reg >= 0 && reg <= 3) {
                // Source address (bytes 0-3)
                u32 src = mem_read32(mem, ADDR_IO_START + 0xB0 + channel * 12);
                dma->source = src & 0x0FFFFFFF;  // Mask to valid address range
            } else if (reg >= 4 && reg <= 7) {
                // Destination address (bytes 4-7)
                u32 dst = mem_read32(mem, ADDR_IO_START + 0xB0 + channel * 12 + 4);
                dma->dest = dst & 0x0FFFFFFF;
            } else if (reg == 8 || reg == 9) {
                // Word count (bytes 8-9)
                u16 count = mem_read16(mem, ADDR_IO_START + 0xB0 + channel * 12 + 8);
                dma->count = count;
            } else if (reg == 10 || reg == 11) {
                // Control register (bytes 10-11)
                u16 control = mem_read16(mem, ADDR_IO_START + 0xB0 + channel * 12 + 10);
                dma_write_control(mem->dma, mem, channel, control);
            }
            return;
        }
        
        // Sound registers: 0x60-0xA7 (stub implementation)
        if (offset >= 0x60 && offset <= 0xA7) {
            // Store sound register writes but don't process them
            // Registers include:
            // 0x60-0x61: SOUND1CNT_L (Channel 1 Sweep)
            // 0x62-0x63: SOUND1CNT_H (Channel 1 Duty/Length/Envelope)
            // 0x64-0x65: SOUND1CNT_X (Channel 1 Frequency/Control)
            // 0x68-0x69: SOUND2CNT_L (Channel 2 Duty/Length/Envelope)
            // 0x6C-0x6D: SOUND2CNT_H (Channel 2 Frequency/Control)
            // 0x70-0x71: SOUND3CNT_L (Channel 3 Stop/Wave RAM select)
            // 0x72-0x73: SOUND3CNT_H (Channel 3 Length/Volume)
            // 0x74-0x75: SOUND3CNT_X (Channel 3 Frequency/Control)
            // 0x78-0x79: SOUND4CNT_L (Channel 4 Length/Envelope)
            // 0x7C-0x7D: SOUND4CNT_H (Channel 4 Frequency/Control)
            // 0x80-0x81: SOUNDCNT_L (Control Stereo/Volume/Enable)
            // 0x82-0x83: SOUNDCNT_H (DMA Sound Control/Mixing)
            // 0x84: SOUNDCNT_X (Sound on/off)
            // 0x88-0x89: SOUNDBIAS (Sound PWM Control)
            // 0x90-0x9F: Wave RAM (Channel 3 waveform)
            // 0xA0-0xA3: FIFO_A (DMA Sound A)
            // 0xA4-0xA7: FIFO_B (DMA Sound B)
            mem->io_regs[offset] = value;
            return;
        }
        
        // Keypad registers: 0x130-0x133 (KEYINPUT is read-only, KEYCNT is writable)
        if (offset >= 0x130 && offset <= 0x133) {
            if (offset == 0x130 || offset == 0x131) {
                // KEYINPUT is read-only - input system writes to io_regs directly
                mem->io_regs[offset] = value;  // Allow for input system updates
            } else {
                // KEYCNT (0x132-0x133) is writable for key interrupt control
                mem->io_regs[offset] = value;
            }
            return;
        }
        
        // Log unknown I/O register writes to help debug initialization
        static u32 io_write_log[512] = {0};
        if (io_write_log[offset] == 0) {
            printf("[I/O] First write to 0x04%06X = 0x%02X\n", offset, value);
            io_write_log[offset] = 1;
        }
        
        mem->io_regs[offset] = value;
        return;
    }
    
    // Palette RAM: 0x05000000 - 0x050003FF
    if (addr >= ADDR_PALETTE_START && addr < ADDR_PALETTE_START + PALETTE_SIZE) {
        mem->palette[addr - ADDR_PALETTE_START] = value;
        
        static bool first_palette_write = true;
        if (first_palette_write && (addr - ADDR_PALETTE_START) < 2) {
            printf("\n*** [BOOT] First palette write! Game entering AgbMain() ***\n\n");
            first_palette_write = false;
        }
        return;
    }
    
    // VRAM: 0x06000000 - 0x06017FFF (mirrors every 128KB up to 0x07000000)
    if (addr >= ADDR_VRAM_START && addr < ADDR_OAM_START) {
        u32 offset = (addr - ADDR_VRAM_START) % (128 * 1024); // 128KB mirror
        if (offset < VRAM_SIZE) {
            mem->vram[offset] = value;
        }
        // Silently ignore writes beyond 96KB within the mirror
        return;
    }
    
    // OAM: 0x07000000 - 0x070003FF
    if (addr >= ADDR_OAM_START && addr < ADDR_OAM_START + OAM_SIZE) {
        mem->oam[addr - ADDR_OAM_START] = value;
        return;
    }
    
    // SRAM/Flash: 0x0E000000 - 0x0E01FFFF (128KB)
    if (addr >= 0x0E000000 && addr < 0x0E020000) {
        u32 offset = addr - 0x0E000000;
        
        // Flash command sequence detection
        if (offset == 0x5555 && value == 0xAA) {
            mem->flash_state = 1;
            static int flash_cmd_count = 0;
            if (flash_cmd_count++ == 0) printf("[FLASH] Command sequence started (0xAA)\n");
            return;
        }
        if (offset == 0x2AAA && value == 0x55 && mem->flash_state == 1) {
            mem->flash_state = 2;
            return;
        }
        if (offset == 0x5555 && mem->flash_state == 2) {
            mem->flash_cmd = value;
            if (value == 0x90) {
                // Enter ID mode
                mem->flash_state = 1;
                printf("[FLASH] Entered ID mode - will return Manufacturer=0xC2, Device=0x09\n");
            } else if (value == 0xF0) {
                // Exit ID/command mode
                mem->flash_state = 0;
                printf("[FLASH] Exited ID mode\n");
            } else if (value == 0xA0) {
                // Byte program mode
                mem->flash_state = 3;
            } else if (value == 0x80) {
                // Erase command prefix
                mem->flash_state = 4;
            } else {
                mem->flash_state = 0;
            }
            return;
        }
        
        // Byte program mode - write data
        if (mem->flash_state == 3) {
            if (offset < sizeof(mem->sram)) {
                mem->sram[offset] = value;
            }
            mem->flash_state = 0;
            return;
        }
        
        // Normal write (shouldn't happen for Flash, but handle it)
        if (offset < sizeof(mem->sram)) {
            mem->sram[offset] = value;
        }
        return;
    }
    
    // ROM is read-only, except for GPIO registers
    if (addr >= ADDR_ROM_START && addr < ADDR_ROM_START + ROM_SIZE) {
        // GPIO registers for RTC
        if (addr == 0x080000C4) {
            mem->gpio_data = (mem->gpio_data & 0xFF00) | value;
            
            static int gpio_write_count = 0;
            if (gpio_write_count < 0) {
                printf("[GPIO] Write to 0x080000C4 (GPIO_DATA) = 0x%02X (full=0x%04X)\n", value, mem->gpio_data);
                gpio_write_count++;
            }
            
            if (mem->rtc) {
                rtc_gpio_write(mem->rtc, mem->gpio_data, mem->gpio_direction);
            }
            return;
        }
        if (addr == 0x080000C5) {
            mem->gpio_data = (mem->gpio_data & 0x00FF) | (value << 8);
            
            static int gpio_write_h_count = 0;
            if (gpio_write_h_count < 5) {
                printf("[GPIO] Write to 0x080000C5 (GPIO_DATA high) = 0x%02X (full=0x%04X)\n", value, mem->gpio_data);
                gpio_write_h_count++;
            }
            
            if (mem->rtc) {
                rtc_gpio_write(mem->rtc, mem->gpio_data, mem->gpio_direction);
            }
            return;
        }
        if (addr == 0x080000C6) {
            mem->gpio_direction = (mem->gpio_direction & 0xFF00) | value;
            
            static int gpio_dir_count = 0;
            if (gpio_dir_count < 10) {
                printf("[GPIO] Write to 0x080000C6 (GPIO_DIRECTION) = 0x%02X (full=0x%04X)\n", value, mem->gpio_direction);
                gpio_dir_count++;
            }
            
            if (mem->rtc) {
                rtc_gpio_write(mem->rtc, mem->gpio_data, mem->gpio_direction);
            }
            return;
        }
        if (addr == 0x080000C7) {
            mem->gpio_direction = (mem->gpio_direction & 0x00FF) | (value << 8);
            
            static int gpio_dir_h_count = 0;
            if (gpio_dir_h_count < 5) {
                printf("[GPIO] Write to 0x080000C7 (GPIO_DIRECTION high) = 0x%02X (full=0x%04X)\n", value, mem->gpio_direction);
                gpio_dir_h_count++;
            }
            
            if (mem->rtc) {
                rtc_gpio_write(mem->rtc, mem->gpio_data, mem->gpio_direction);
            }
            return;
        }
        if (addr == 0x080000C8) {
            mem->gpio_control = (mem->gpio_control & 0xFF00) | value;
            
            static int gpio_ctrl_count = 0;
            if (gpio_ctrl_count < 5) {
                printf("[GPIO] Write to 0x080000C8 (GPIO_CONTROL) = 0x%02X (full=0x%04X)\n", value, mem->gpio_control);
                gpio_ctrl_count++;
            }
            return;
        }
        if (addr == 0x080000C9) {
            mem->gpio_control = (mem->gpio_control & 0x00FF) | (value << 8);
            
            static int gpio_ctrl_h_count = 0;
            if (gpio_ctrl_h_count < 5) {
                printf("[GPIO] Write to 0x080000C9 (GPIO_CONTROL high) = 0x%02X (full=0x%04X)\n", value, mem->gpio_control);
                gpio_ctrl_h_count++;
            }
            return;
        }
        
        // Silently ignore ROM writes (likely debug or test code)
        // fprintf(stderr, "Warning: Attempted write to ROM at 0x%08X\n", addr);
        return;
    }
    
    // Extended I/O registers (beyond 0x04000400)
    if (addr >= 0x04000400 && addr < 0x05000000) {
        // WAITCNT (0x04000204) - Wait State Control
        if (addr == 0x04000204 || addr == 0x04000205) {
            // Accept but ignore wait state configuration
            // Games write this to optimize ROM/SRAM access timing
            return;
        }
        // Other extended I/O - accept silently
        return;
    }
    
    // Save type detection probes (0x09000000-0x0DFFFFFF, 0x10000000+)
    // Games write to these addresses to detect save memory type
    // SRAM is at 0x0E000000, but games probe other addresses too
    if ((addr >= 0x09000000 && addr < 0x0E000000) || addr >= 0x10000000) {
        // Silently ignore - these are save detection probes
        static int probe_count = 0;
        if (probe_count < 3) {
            printf("[SAVE_DETECT] Probe write to 0x%08X = 0x%02X (detection test)\n", addr, value);
            probe_count++;
        }
        return;
    }
    
    fprintf(stderr, "Warning: Write to unmapped address 0x%08X = 0x%02X\n", addr, value);
}

void mem_write16(Memory *mem, u32 addr, u16 value) {
    // Debug: Log palette writes
    static bool logged_palette = false;
    if (!logged_palette && addr >= 0x05000000 && addr < 0x05000400) {
        printf("\n*** [BOOT-16] Palette write16 to 0x%08X = 0x%04X ***\n\n", addr, value);
        logged_palette = true;
    }
    
    mem_write8(mem, addr, (u8)(value & 0xFF));
    mem_write8(mem, addr + 1, (u8)((value >> 8) & 0xFF));
}

void mem_write32(Memory *mem, u32 addr, u32 value) {
    mem_write16(mem, addr, (u16)(value & 0xFFFF));
    mem_write16(mem, addr + 2, (u16)((value >> 16) & 0xFFFF));
}

u8 mem_get_ai_input(Memory *mem) {
    // gAiInputState is at 0x0203cf64
    // Offset from EWRAM base: 0x3cf64
    return mem->ewram[0x3cf64];
}

void mem_set_ai_input(Memory *mem, u8 value) {
    mem->ewram[0x3cf64] = value;
}
