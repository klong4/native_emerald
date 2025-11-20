#include "cpu_core.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Helper macros for ARM instruction decoding
#define ARM_COND(op) ((op) >> 28)
#define ARM_OP(op) (((op) >> 25) & 0x7)
#define ARM_RD(op) (((op) >> 12) & 0xF)
#define ARM_RN(op) (((op) >> 16) & 0xF)
#define ARM_RM(op) ((op) & 0xF)
#define ARM_RS(op) (((op) >> 8) & 0xF)
#define ARM_IMM(op) ((op) & 0xFF)
#define ARM_ROTATE(op) (((op) >> 8) & 0xF)
#define ARM_OFFSET12(op) ((op) & 0xFFF)
#define ARM_OFFSET24(op) ((op) & 0xFFFFFF)

// Condition codes
#define COND_EQ 0x0  // Z set
#define COND_NE 0x1  // Z clear
#define COND_CS 0x2  // C set
#define COND_CC 0x3  // C clear
#define COND_MI 0x4  // N set
#define COND_PL 0x5  // N clear
#define COND_VS 0x6  // V set
#define COND_VC 0x7  // V clear
#define COND_HI 0x8  // C set and Z clear
#define COND_LS 0x9  // C clear or Z set
#define COND_GE 0xA  // N == V
#define COND_LT 0xB  // N != V
#define COND_GT 0xC  // Z clear and N == V
#define COND_LE 0xD  // Z set or N != V
#define COND_AL 0xE  // Always
#define COND_NV 0xF  // Never (deprecated)

// Thumb instruction helpers
#define THUMB_OP(op) (((op) >> 13) & 0x7)
#define THUMB_RD(op) ((op) & 0x7)
#define THUMB_RS(op) (((op) >> 3) & 0x7)
#define THUMB_RN(op) (((op) >> 6) & 0x7)
#define THUMB_IMM3(op) (((op) >> 6) & 0x7)
#define THUMB_IMM5(op) (((op) >> 6) & 0x1F)
#define THUMB_IMM8(op) ((op) & 0xFF)
#define THUMB_OFFSET5(op) (((op) >> 6) & 0x1F)
#define THUMB_OFFSET8(op) ((op) & 0xFF)
#define THUMB_OFFSET11(op) ((op) & 0x7FF)

void cpu_init(ARM7TDMI *cpu) {
    if (!cpu) return;
    
    memset(cpu->r, 0, sizeof(cpu->r));
    cpu->cpsr = 0;
    cpu->spsr = 0;
    cpu->thumb_mode = false;
    cpu->cycles = 0;
    cpu->halted = false;
}

void cpu_reset(ARM7TDMI *cpu) {
    if (!cpu) return;
    
    cpu_init(cpu);
    
    // GBA starts at 0x08000000 in ARM mode
    cpu->r[15] = ADDR_ROM_START;  // PC
    cpu->r[13] = 0x03007F00;      // SP (default)
    cpu->thumb_mode = false;
    cpu->cpsr = 0x1F;  // System mode
}

void cpu_set_flag(ARM7TDMI *cpu, u32 flag) {
    cpu->cpsr |= flag;
}

void cpu_clear_flag(ARM7TDMI *cpu, u32 flag) {
    cpu->cpsr &= ~flag;
}

bool cpu_get_flag(ARM7TDMI *cpu, u32 flag) {
    return (cpu->cpsr & flag) != 0;
}

static inline bool check_condition(ARM7TDMI *cpu, u32 cond) {
    bool n = cpu->cpsr & FLAG_N;
    bool z = cpu->cpsr & FLAG_Z;
    bool c = cpu->cpsr & FLAG_C;
    bool v = cpu->cpsr & FLAG_V;
    
    switch (cond) {
        case COND_EQ: return z;
        case COND_NE: return !z;
        case COND_CS: return c;
        case COND_CC: return !c;
        case COND_MI: return n;
        case COND_PL: return !n;
        case COND_VS: return v;
        case COND_VC: return !v;
        case COND_HI: return c && !z;
        case COND_LS: return !c || z;
        case COND_GE: return n == v;
        case COND_LT: return n != v;
        case COND_GT: return !z && (n == v);
        case COND_LE: return z || (n != v);
        case COND_AL: return true;
        case COND_NV: return false;
    }
    return false;
}

static void update_flags_logical(ARM7TDMI *cpu, u32 result) {
    if (result == 0) cpu->cpsr |= FLAG_Z;
    else cpu->cpsr &= ~FLAG_Z;
    
    if (result & 0x80000000) cpu->cpsr |= FLAG_N;
    else cpu->cpsr &= ~FLAG_N;
}

static void update_flags_add(ARM7TDMI *cpu, u32 a, u32 b, u32 result) {
    update_flags_logical(cpu, result);
    
    // Carry
    if (result < a) cpu->cpsr |= FLAG_C;
    else cpu->cpsr &= ~FLAG_C;
    
    // Overflow (signed overflow)
    bool overflow = ((a & 0x80000000) == (b & 0x80000000)) && 
                    ((a & 0x80000000) != (result & 0x80000000));
    if (overflow) cpu->cpsr |= FLAG_V;
    else cpu->cpsr &= ~FLAG_V;
}

static void update_flags_sub(ARM7TDMI *cpu, u32 a, u32 b, u32 result) {
    update_flags_logical(cpu, result);
    
    // Carry (borrow)
    if (a >= b) cpu->cpsr |= FLAG_C;
    else cpu->cpsr &= ~FLAG_C;
    
    // Overflow
    bool overflow = ((a & 0x80000000) != (b & 0x80000000)) && 
                    ((a & 0x80000000) != (result & 0x80000000));
    if (overflow) cpu->cpsr |= FLAG_V;
    else cpu->cpsr &= ~FLAG_V;
}

// Barrel shifter for ARM instructions
static u32 barrel_shift(ARM7TDMI *cpu, u32 value, u32 shift_type, u32 shift_amount, bool set_carry) {
    u32 result = value;
    bool carry = cpu->cpsr & FLAG_C;
    
    switch (shift_type) {
        case 0: // LSL
            if (shift_amount == 0) {
                result = value;
            } else if (shift_amount < 32) {
                if (set_carry) carry = (value >> (32 - shift_amount)) & 1;
                result = value << shift_amount;
            } else if (shift_amount == 32) {
                if (set_carry) carry = value & 1;
                result = 0;
            } else {
                if (set_carry) carry = false;
                result = 0;
            }
            break;
        case 1: // LSR
            if (shift_amount == 0 || shift_amount == 32) {
                if (set_carry) carry = (value >> 31) & 1;
                result = 0;
            } else if (shift_amount < 32) {
                if (set_carry) carry = (value >> (shift_amount - 1)) & 1;
                result = value >> shift_amount;
            } else {
                if (set_carry) carry = false;
                result = 0;
            }
            break;
        case 2: // ASR
            if (shift_amount == 0 || shift_amount >= 32) {
                if (value & 0x80000000) {
                    result = 0xFFFFFFFF;
                    if (set_carry) carry = true;
                } else {
                    result = 0;
                    if (set_carry) carry = false;
                }
            } else {
                if (set_carry) carry = ((s32)value >> (shift_amount - 1)) & 1;
                result = (s32)value >> shift_amount;
            }
            break;
        case 3: // ROR or RRX
            if (shift_amount == 0) { // RRX
                if (set_carry) {
                    bool old_carry = carry;
                    carry = value & 1;
                    result = (value >> 1) | (old_carry ? 0x80000000 : 0);
                }
            } else {
                shift_amount &= 31;
                if (shift_amount == 0) {
                    result = value;
                } else {
                    if (set_carry) carry = (value >> (shift_amount - 1)) & 1;
                    result = (value >> shift_amount) | (value << (32 - shift_amount));
                }
            }
            break;
    }
    
    if (set_carry) {
        if (carry) cpu->cpsr |= FLAG_C;
        else cpu->cpsr &= ~FLAG_C;
    }
    
    return result;
}

// ARM instruction execution
static u32 execute_arm(ARM7TDMI *cpu, Memory *mem, u32 opcode) {
    u32 cond = ARM_COND(opcode);
    
    if (!check_condition(cpu, cond)) {
        return 1; // Instruction not executed
    }
    
    u32 op_type = ARM_OP(opcode);
    
    // Multiply (000x with bits 4-7 = 1001)
    if ((opcode & 0x0FC000F0) == 0x00000090) {
        bool accumulate = opcode & (1 << 21);
        bool set_flags = opcode & (1 << 20);
        u32 rd = ARM_RD(opcode);
        u32 rn = ARM_RN(opcode);
        u32 rs = ARM_RS(opcode);
        u32 rm = ARM_RM(opcode);
        
        u32 result = cpu->r[rm] * cpu->r[rs];
        if (accumulate) result += cpu->r[rn];
        
        cpu->r[rd] = result;
        if (set_flags) {
            update_flags_logical(cpu, result);
        }
        return 2;
    }
    
    // Single data swap (000x with bits 4-7 = 1001, bit 24 = 1)
    if ((opcode & 0x0FB00FF0) == 0x01000090) {
        bool byte = opcode & (1 << 22);
        u32 rn = ARM_RN(opcode);
        u32 rd = ARM_RD(opcode);
        u32 rm = ARM_RM(opcode);
        
        u32 addr = cpu->r[rn];
        if (byte) {
            u32 temp = mem_read8(mem, addr);
            mem_write8(mem, addr, cpu->r[rm] & 0xFF);
            cpu->r[rd] = temp;
        } else {
            u32 temp = mem_read32(mem, addr & ~3);
            mem_write32(mem, addr & ~3, cpu->r[rm]);
            cpu->r[rd] = temp;
        }
        return 4;
    }
    
    // Data processing and immediate operations (00x)
    if ((op_type & 0x6) == 0) {
        bool immediate = opcode & (1 << 25);
        bool set_flags = opcode & (1 << 20);
        u32 opcode_type = (opcode >> 21) & 0xF;
        u32 rn = ARM_RN(opcode);
        u32 rd = ARM_RD(opcode);
        
        u32 operand2;
        if (immediate) {
            u32 imm = ARM_IMM(opcode);
            u32 rotate = ARM_ROTATE(opcode) * 2;
            if (rotate == 0) {
                operand2 = imm;
            } else {
                operand2 = (imm >> rotate) | (imm << (32 - rotate));
                if (set_flags && (opcode_type & 0xC) != 0x8) { // Not TST/TEQ/CMP/CMN
                    bool carry = (imm >> (rotate - 1)) & 1;
                    if (carry) cpu->cpsr |= FLAG_C;
                    else cpu->cpsr &= ~FLAG_C;
                }
            }
        } else {
            u32 rm = ARM_RM(opcode);
            u32 shift = (opcode >> 4) & 0xFF;
            u32 shift_type = (shift >> 1) & 3;
            u32 shift_amount;
            
            if (shift & 1) { // Register shift
                u32 rs = (shift >> 4) & 0xF;
                shift_amount = cpu->r[rs] & 0xFF;
            } else { // Immediate shift
                shift_amount = (shift >> 3) & 0x1F;
            }
            
            operand2 = barrel_shift(cpu, cpu->r[rm], shift_type, shift_amount, 
                                   set_flags && (opcode_type & 0xC) != 0x8);
        }
        
        u32 op1 = cpu->r[rn];
        u32 result = 0;
        
        switch (opcode_type) {
            case 0x0: // AND
                result = op1 & operand2;
                if (rd != 15) cpu->r[rd] = result;
                if (set_flags) update_flags_logical(cpu, result);
                break;
            case 0x1: // EOR
                result = op1 ^ operand2;
                if (rd != 15) cpu->r[rd] = result;
                if (set_flags) update_flags_logical(cpu, result);
                break;
            case 0x2: // SUB
                result = op1 - operand2;
                if (rd != 15) cpu->r[rd] = result;
                if (set_flags) update_flags_sub(cpu, op1, operand2, result);
                break;
            case 0x3: // RSB
                result = operand2 - op1;
                if (rd != 15) cpu->r[rd] = result;
                if (set_flags) update_flags_sub(cpu, operand2, op1, result);
                break;
            case 0x4: // ADD
                result = op1 + operand2;
                if (rd != 15) cpu->r[rd] = result;
                if (set_flags) update_flags_add(cpu, op1, operand2, result);
                break;
            case 0x5: // ADC
                result = op1 + operand2 + ((cpu->cpsr & FLAG_C) ? 1 : 0);
                if (rd != 15) cpu->r[rd] = result;
                if (set_flags) update_flags_add(cpu, op1, operand2, result);
                break;
            case 0x6: // SBC
                result = op1 - operand2 - ((cpu->cpsr & FLAG_C) ? 0 : 1);
                if (rd != 15) cpu->r[rd] = result;
                if (set_flags) update_flags_sub(cpu, op1, operand2, result);
                break;
            case 0x7: // RSC
                result = operand2 - op1 - ((cpu->cpsr & FLAG_C) ? 0 : 1);
                if (rd != 15) cpu->r[rd] = result;
                if (set_flags) update_flags_sub(cpu, operand2, op1, result);
                break;
            case 0x8: // TST
                result = op1 & operand2;
                update_flags_logical(cpu, result);
                break;
            case 0x9: // TEQ
                result = op1 ^ operand2;
                update_flags_logical(cpu, result);
                break;
            case 0xA: // CMP
                result = op1 - operand2;
                update_flags_sub(cpu, op1, operand2, result);
                break;
            case 0xB: // CMN
                result = op1 + operand2;
                update_flags_add(cpu, op1, operand2, result);
                break;
            case 0xC: // ORR
                result = op1 | operand2;
                if (rd != 15) cpu->r[rd] = result;
                if (set_flags) update_flags_logical(cpu, result);
                break;
            case 0xD: // MOV
                result = operand2;
                if (rd != 15) cpu->r[rd] = result;
                if (set_flags) update_flags_logical(cpu, result);
                break;
            case 0xE: // BIC
                result = op1 & ~operand2;
                if (rd != 15) cpu->r[rd] = result;
                if (set_flags) update_flags_logical(cpu, result);
                break;
            case 0xF: // MVN
                result = ~operand2;
                if (rd != 15) cpu->r[rd] = result;
                if (set_flags) update_flags_logical(cpu, result);
                break;
        }
        
        if (rd == 15) {
            cpu->r[15] = result & 0xFFFFFFFE;
            cpu->thumb_mode = result & 1;
        }
        
        return 1;
    }
    
    // Load/Store (01x)
    if ((op_type & 0x6) == 0x2) {
        bool immediate = !(opcode & (1 << 25));
        bool pre_index = opcode & (1 << 24);
        bool up = opcode & (1 << 23);
        bool byte = opcode & (1 << 22);
        bool writeback = opcode & (1 << 21);
        bool load = opcode & (1 << 20);
        u32 rn = ARM_RN(opcode);
        u32 rd = ARM_RD(opcode);
        
        u32 offset;
        if (immediate) {
            offset = ARM_OFFSET12(opcode);
        } else {
            u32 rm = ARM_RM(opcode);
            u32 shift = (opcode >> 4) & 0xFF;
            u32 shift_type = (shift >> 1) & 3;
            u32 shift_amount = (shift >> 3) & 0x1F;
            offset = barrel_shift(cpu, cpu->r[rm], shift_type, shift_amount, false);
        }
        
        u32 addr = cpu->r[rn];
        if (pre_index) {
            if (up) addr += offset;
            else addr -= offset;
        }
        
        if (load) {
            if (byte) {
                cpu->r[rd] = mem_read8(mem, addr);
            } else {
                cpu->r[rd] = mem_read32(mem, addr & ~3);
                if (addr & 3) {
                    u32 rotate = (addr & 3) * 8;
                    cpu->r[rd] = (cpu->r[rd] >> rotate) | (cpu->r[rd] << (32 - rotate));
                }
            }
        } else {
            if (byte) {
                mem_write8(mem, addr, cpu->r[rd] & 0xFF);
            } else {
                mem_write32(mem, addr & ~3, cpu->r[rd]);
            }
        }
        
        if (!pre_index) {
            if (up) cpu->r[rn] += offset;
            else cpu->r[rn] -= offset;
        } else if (writeback) {
            cpu->r[rn] = addr;
        }
        
        return 3;
    }
    
    // Block data transfer (100)
    if (op_type == 0x4) {
        bool pre_index = opcode & (1 << 24);
        bool up = opcode & (1 << 23);
        bool load_psr = opcode & (1 << 22);
        bool writeback = opcode & (1 << 21);
        bool load = opcode & (1 << 20);
        u32 rn = ARM_RN(opcode);
        u32 rlist = opcode & 0xFFFF;
        
        u32 addr = cpu->r[rn];
        int count = 0;
        for (int i = 0; i < 16; i++) {
            if (rlist & (1 << i)) count++;
        }
        
        if (!up) addr -= count * 4;
        
        u32 start_addr = addr;
        
        for (int i = 0; i < 16; i++) {
            if (rlist & (1 << i)) {
                if (pre_index) addr += 4;
                
                if (load) {
                    cpu->r[i] = mem_read32(mem, addr & ~3);
                } else {
                    mem_write32(mem, addr & ~3, cpu->r[i]);
                }
                
                if (!pre_index) addr += 4;
            }
        }
        
        if (writeback) {
            if (up) cpu->r[rn] = start_addr + count * 4;
            else cpu->r[rn] = start_addr;
        }
        
        (void)load_psr; // TODO: Handle PSR transfer
        return count + 2;
    }
    
    // Branch (101)
    if (op_type == 0x5) {
        bool link = opcode & (1 << 24);
        s32 offset = (s32)(ARM_OFFSET24(opcode) << 8) >> 6; // Sign extend and *4
        
        if (link) {
            cpu->r[14] = cpu->r[15] - 4; // Save return address
        }
        
        cpu->r[15] = (cpu->r[15] + offset) & 0xFFFFFFFE;
        return 3;
    }
    
    // Halfword/signed data transfer (000x with bit 7=1, bit 4=1)
    if ((opcode & 0x0E000090) == 0x00000090 && (opcode & 0x60)) {
        bool pre_index = opcode & (1 << 24);
        bool up = opcode & (1 << 23);
        bool immediate = opcode & (1 << 22);
        bool writeback = opcode & (1 << 21);
        bool load = opcode & (1 << 20);
        u32 rn = ARM_RN(opcode);
        u32 rd = ARM_RD(opcode);
        u32 sh = (opcode >> 5) & 3;
        
        u32 offset;
        if (immediate) {
            offset = ((opcode >> 4) & 0xF0) | (opcode & 0xF);
        } else {
            u32 rm = ARM_RM(opcode);
            offset = cpu->r[rm];
        }
        
        u32 addr = cpu->r[rn];
        if (pre_index) {
            if (up) addr += offset;
            else addr -= offset;
        }
        
        if (load) {
            switch (sh) {
                case 1: // LDRH
                    cpu->r[rd] = mem_read16(mem, addr & ~1);
                    break;
                case 2: // LDRSB
                    cpu->r[rd] = (s32)(s8)mem_read8(mem, addr);
                    break;
                case 3: // LDRSH
                    cpu->r[rd] = (s32)(s16)mem_read16(mem, addr & ~1);
                    break;
            }
        } else {
            if (sh == 1) { // STRH
                mem_write16(mem, addr & ~1, cpu->r[rd] & 0xFFFF);
            }
        }
        
        if (!pre_index) {
            if (up) cpu->r[rn] += offset;
            else cpu->r[rn] -= offset;
        } else if (writeback) {
            cpu->r[rn] = addr;
        }
        
        return 3;
    }
    
    // Branch and exchange (BX)
    if ((opcode & 0x0FFFFFF0) == 0x012FFF10) {
        u32 rn = opcode & 0xF;
        u32 addr = cpu->r[rn];
        cpu->thumb_mode = addr & 1;
        cpu->r[15] = addr & 0xFFFFFFFE;
        return 3;
    }
    
    // SWI
    if (op_type == 0x7 && (opcode & 0x0F000000) == 0x0F000000) {
        // Software interrupt - BIOS High-Level Emulation
        u32 comment = opcode & 0xFF;
        
        switch (comment) {
            case 0x00: // SoftReset
                cpu->r[13] = 0x03007F00;
                cpu->r[15] = 0x08000000;
                cpu->cpsr = 0x000000D3;
                break;
                
            case 0x02: // Halt
            case 0x03: // Stop
            case 0x04: // IntrWait
            case 0x05: // VBlankIntrWait
                // Just continue for now
                break;
                
            case 0x06: // Div
                {
                    s32 num = (s32)cpu->r[0];
                    s32 denom = (s32)cpu->r[1];
                    if (denom != 0) {
                        cpu->r[0] = (u32)(num / denom);
                        cpu->r[1] = (u32)(num % denom);
                        s32 abs_num = num < 0 ? -num : num;
                        s32 abs_denom = denom < 0 ? -denom : denom;
                        cpu->r[3] = (u32)(abs_num / abs_denom);
                    } else {
                        cpu->r[0] = 0;
                        cpu->r[1] = 0;
                        cpu->r[3] = 0;
                    }
                }
                break;
                
            case 0x08: // Sqrt
                {
                    u32 val = cpu->r[0];
                    u32 result = 0;
                    u32 bit = 1 << 30;
                    while (bit > val) bit >>= 2;
                    while (bit != 0) {
                        if (val >= result + bit) {
                            val -= result + bit;
                            result = (result >> 1) + bit;
                        } else {
                            result >>= 1;
                        }
                        bit >>= 2;
                    }
                    cpu->r[0] = result;
                }
                break;
                
            case 0x0B: // CpuSet
                {
                    u32 src = cpu->r[0];
                    u32 dst = cpu->r[1];
                    u32 len_mode = cpu->r[2];
                    u32 count = len_mode & 0x1FFFFF;
                    bool fixed_src = len_mode & (1 << 24);
                    bool word_size = len_mode & (1 << 26);
                    
                    if (word_size) {
                        for (u32 i = 0; i < count; i++) {
                            u32 value = mem_read32(mem, src);
                            mem_write32(mem, dst, value);
                            if (!fixed_src) src += 4;
                            dst += 4;
                        }
                    } else {
                        for (u32 i = 0; i < count; i++) {
                            u16 value = mem_read16(mem, src);
                            mem_write16(mem, dst, value);
                            if (!fixed_src) src += 2;
                            dst += 2;
                        }
                    }
                }
                break;
                
            case 0x0C: // CpuFastSet
                {
                    u32 src = cpu->r[0];
                    u32 dst = cpu->r[1];
                    u32 len_mode = cpu->r[2];
                    u32 count = len_mode & 0x1FFFFF;
                    bool fixed_src = len_mode & (1 << 24);
                    
                    for (u32 i = 0; i < count; i++) {
                        u32 value = mem_read32(mem, src);
                        mem_write32(mem, dst, value);
                        if (!fixed_src) src += 4;
                        dst += 4;
                    }
                }
                break;
                
            case 0x0D: // GetBiosChecksum
                cpu->r[0] = 0xBAAE187F;
                break;
                
            case 0x11: // LZ77UnCompWram
            case 0x12: // LZ77UnCompVram
                {
                    u32 src = cpu->r[0];
                    u32 dst = cpu->r[1];
                    u32 header = mem_read32(mem, src);
                    u32 size = header >> 8;
                    src += 4;
                    
                    u32 dst_pos = 0;
                    while (dst_pos < size) {
                        u8 flags = mem_read8(mem, src++);
                        for (int i = 0; i < 8 && dst_pos < size; i++) {
                            if (flags & (0x80 >> i)) {
                                u8 b1 = mem_read8(mem, src++);
                                u8 b2 = mem_read8(mem, src++);
                                u32 len = (b1 >> 4) + 3;
                                u32 disp = (((b1 & 0xF) << 8) | b2) + 1;
                                for (u32 j = 0; j < len && dst_pos < size; j++) {
                                    u8 byte = mem_read8(mem, dst + dst_pos - disp);
                                    mem_write8(mem, dst + dst_pos, byte);
                                    dst_pos++;
                                }
                            } else {
                                u8 byte = mem_read8(mem, src++);
                                mem_write8(mem, dst + dst_pos, byte);
                                dst_pos++;
                            }
                        }
                    }
                }
                break;
                
            case 0x14: // RLUnCompWram
            case 0x15: // RLUnCompVram
                {
                    u32 src = cpu->r[0];
                    u32 dst = cpu->r[1];
                    u32 header = mem_read32(mem, src);
                    u32 size = header >> 8;
                    src += 4;
                    
                    u32 dst_pos = 0;
                    while (dst_pos < size) {
                        u8 flag = mem_read8(mem, src++);
                        if (flag & 0x80) {
                            u32 len = (flag & 0x7F) + 3;
                            u8 data = mem_read8(mem, src++);
                            for (u32 i = 0; i < len && dst_pos < size; i++) {
                                mem_write8(mem, dst + dst_pos++, data);
                            }
                        } else {
                            u32 len = (flag & 0x7F) + 1;
                            for (u32 i = 0; i < len && dst_pos < size; i++) {
                                u8 data = mem_read8(mem, src++);
                                mem_write8(mem, dst + dst_pos++, data);
                            }
                        }
                    }
                }
                break;
                
            default:
                // Unknown BIOS call - just return
                break;
        }
        
        return 3;
    }
    
    // MRS - Move PSR to register
    if ((opcode & 0x0FBF0FFF) == 0x010F0000) {
        u32 rd = ARM_RD(opcode);
        bool spsr = opcode & (1 << 22);
        
        if (spsr) {
            // SPSR not implemented yet - use CPSR
            cpu->r[rd] = cpu->cpsr;
        } else {
            cpu->r[rd] = cpu->cpsr;
        }
        return 1;
    }
    
    // MSR - Move register to PSR
    if (((opcode & 0x0FB00000) == 0x03200000) || ((opcode & 0x0DB00000) == 0x01200000)) {
        bool immediate = opcode & (1 << 25);
        bool spsr = opcode & (1 << 22);
        u32 field_mask = 0;
        
        if (opcode & (1 << 16)) field_mask |= 0x000000FF; // Control field
        if (opcode & (1 << 17)) field_mask |= 0x0000FF00; // Extension field
        if (opcode & (1 << 18)) field_mask |= 0x00FF0000; // Status field
        if (opcode & (1 << 19)) field_mask |= 0xFF000000; // Flags field
        
        u32 value;
        if (immediate) {
            u32 imm = ARM_IMM(opcode);
            u32 rotate = ARM_ROTATE(opcode) * 2;
            value = (imm >> rotate) | (imm << (32 - rotate));
        } else {
            value = cpu->r[ARM_RM(opcode)];
        }
        
        if (spsr) {
            // SPSR not implemented - ignore
        } else {
            cpu->cpsr = (cpu->cpsr & ~field_mask) | (value & field_mask);
        }
        return 1;
    }
    
    // Coprocessor instructions - stub them out
    // CDP - Coprocessor data processing
    if ((opcode & 0x0F000010) == 0x0E000000) {
        // Ignore coprocessor operations
        return 1;
    }
    
    // LDC/STC - Coprocessor load/store
    if ((opcode & 0x0E000000) == 0x0C000000) {
        // Ignore coprocessor transfers
        return 1;
    }
    
    // MCR/MRC - Coprocessor register transfer
    if ((opcode & 0x0F000010) == 0x0E000010) {
        // Ignore coprocessor register transfers
        return 1;
    }
    
    // Unimplemented instruction
    return 1;
}

// Thumb instruction execution
static u32 execute_thumb(ARM7TDMI *cpu, Memory *mem, u16 opcode) {
    // Move shifted register (000xx)
    if (((opcode >> 13) & 0x7) <= 0x1) {
        u32 offset = THUMB_OFFSET5(opcode);
        u32 rs = THUMB_RS(opcode);
        u32 rd = THUMB_RD(opcode);
        u32 shift_type = (opcode >> 11) & 0x3;
        u32 result;
        
        switch (shift_type) {
            case 0: // LSL
                result = cpu->r[rs] << offset;
                break;
            case 1: // LSR
                result = offset ? (cpu->r[rs] >> offset) : 0;
                break;
            case 2: // ASR
                result = offset ? ((s32)cpu->r[rs] >> offset) : ((cpu->r[rs] & 0x80000000) ? 0xFFFFFFFF : 0);
                break;
            default:
                result = cpu->r[rs];
        }
        
        cpu->r[rd] = result;
        update_flags_logical(cpu, result);
        return 1;
    }
    
    // Add/subtract (00011)
    if (((opcode >> 11) & 0x1F) == 0x3) {
        bool immediate = opcode & (1 << 10);
        bool subtract = opcode & (1 << 9);
        u32 rn_imm = THUMB_RN(opcode);
        u32 rs = THUMB_RS(opcode);
        u32 rd = THUMB_RD(opcode);
        
        u32 operand = immediate ? rn_imm : cpu->r[rn_imm];
        u32 result;
        
        if (subtract) {
            result = cpu->r[rs] - operand;
            update_flags_sub(cpu, cpu->r[rs], operand, result);
        } else {
            result = cpu->r[rs] + operand;
            update_flags_add(cpu, cpu->r[rs], operand, result);
        }
        
        cpu->r[rd] = result;
        return 1;
    }
    
    // Move/compare/add/subtract immediate (001xx)
    if (((opcode >> 13) & 0x7) == 0x1) {
        u32 op_type = (opcode >> 11) & 0x3;
        u32 rd = (opcode >> 8) & 0x7;
        u32 imm = THUMB_IMM8(opcode);
        u32 result;
        
        switch (op_type) {
            case 0: // MOV
                cpu->r[rd] = imm;
                update_flags_logical(cpu, imm);
                break;
            case 1: // CMP
                result = cpu->r[rd] - imm;
                update_flags_sub(cpu, cpu->r[rd], imm, result);
                break;
            case 2: // ADD
                result = cpu->r[rd] + imm;
                update_flags_add(cpu, cpu->r[rd], imm, result);
                cpu->r[rd] = result;
                break;
            case 3: // SUB
                result = cpu->r[rd] - imm;
                update_flags_sub(cpu, cpu->r[rd], imm, result);
                cpu->r[rd] = result;
                break;
        }
        return 1;
    }
    
    // ALU operations (010000)
    if (((opcode >> 10) & 0x3F) == 0x10) {
        u32 op = (opcode >> 6) & 0xF;
        u32 rs = THUMB_RS(opcode);
        u32 rd = THUMB_RD(opcode);
        u32 result;
        
        switch (op) {
            case 0x0: // AND
                cpu->r[rd] &= cpu->r[rs];
                update_flags_logical(cpu, cpu->r[rd]);
                break;
            case 0x1: // EOR
                cpu->r[rd] ^= cpu->r[rs];
                update_flags_logical(cpu, cpu->r[rd]);
                break;
            case 0x2: // LSL
                result = cpu->r[rd] << (cpu->r[rs] & 0xFF);
                cpu->r[rd] = result;
                update_flags_logical(cpu, result);
                break;
            case 0x3: // LSR
                result = cpu->r[rd] >> (cpu->r[rs] & 0xFF);
                cpu->r[rd] = result;
                update_flags_logical(cpu, result);
                break;
            case 0x4: // ASR
                result = (s32)cpu->r[rd] >> (cpu->r[rs] & 0xFF);
                cpu->r[rd] = result;
                update_flags_logical(cpu, result);
                break;
            case 0x5: // ADC
                result = cpu->r[rd] + cpu->r[rs] + ((cpu->cpsr & FLAG_C) ? 1 : 0);
                update_flags_add(cpu, cpu->r[rd], cpu->r[rs], result);
                cpu->r[rd] = result;
                break;
            case 0x6: // SBC
                result = cpu->r[rd] - cpu->r[rs] - ((cpu->cpsr & FLAG_C) ? 0 : 1);
                update_flags_sub(cpu, cpu->r[rd], cpu->r[rs], result);
                cpu->r[rd] = result;
                break;
            case 0x7: // ROR
                {
                    u32 shift = cpu->r[rs] & 0xFF;
                    if (shift) {
                        shift &= 31;
                        result = (cpu->r[rd] >> shift) | (cpu->r[rd] << (32 - shift));
                    } else {
                        result = cpu->r[rd];
                    }
                    cpu->r[rd] = result;
                    update_flags_logical(cpu, result);
                }
                break;
            case 0x8: // TST
                result = cpu->r[rd] & cpu->r[rs];
                update_flags_logical(cpu, result);
                break;
            case 0x9: // NEG
                result = 0 - cpu->r[rs];
                update_flags_sub(cpu, 0, cpu->r[rs], result);
                cpu->r[rd] = result;
                break;
            case 0xA: // CMP
                result = cpu->r[rd] - cpu->r[rs];
                update_flags_sub(cpu, cpu->r[rd], cpu->r[rs], result);
                break;
            case 0xB: // CMN
                result = cpu->r[rd] + cpu->r[rs];
                update_flags_add(cpu, cpu->r[rd], cpu->r[rs], result);
                break;
            case 0xC: // ORR
                cpu->r[rd] |= cpu->r[rs];
                update_flags_logical(cpu, cpu->r[rd]);
                break;
            case 0xD: // MUL
                result = cpu->r[rd] * cpu->r[rs];
                cpu->r[rd] = result;
                update_flags_logical(cpu, result);
                return 2;
            case 0xE: // BIC
                cpu->r[rd] &= ~cpu->r[rs];
                update_flags_logical(cpu, cpu->r[rd]);
                break;
            case 0xF: // MVN
                cpu->r[rd] = ~cpu->r[rs];
                update_flags_logical(cpu, cpu->r[rd]);
                break;
        }
        return 1;
    }
    
    // Hi register operations/branch exchange (010001)
    if (((opcode >> 10) & 0x3F) == 0x11) {
        u32 op = (opcode >> 8) & 0x3;
        bool h1 = (opcode >> 7) & 1;
        bool h2 = (opcode >> 6) & 1;
        u32 rs = ((opcode >> 3) & 0x7) | (h2 ? 8 : 0);
        u32 rd = (opcode & 0x7) | (h1 ? 8 : 0);
        
        switch (op) {
            case 0: // ADD
                cpu->r[rd] += cpu->r[rs];
                if (rd == 15) cpu->r[15] &= ~1;
                break;
            case 1: // CMP
                {
                    u32 result = cpu->r[rd] - cpu->r[rs];
                    update_flags_sub(cpu, cpu->r[rd], cpu->r[rs], result);
                }
                break;
            case 2: // MOV
                cpu->r[rd] = cpu->r[rs];
                if (rd == 15) cpu->r[15] &= ~1;
                break;
            case 3: // BX/BLX
                {
                    u32 addr = cpu->r[rs];
                    cpu->thumb_mode = addr & 1;
                    cpu->r[15] = addr & 0xFFFFFFFE;
                }
                return 3;
        }
        return 1;
    }
    
    // PC-relative load (01001)
    if (((opcode >> 11) & 0x1F) == 0x9) {
        u32 rd = (opcode >> 8) & 0x7;
        u32 offset = (opcode & 0xFF) << 2;
        u32 addr = (cpu->r[15] & ~2) + offset;
        cpu->r[rd] = mem_read32(mem, addr & ~3);
        return 3;
    }
    
    // Load/store with register offset (0101xx)
    if (((opcode >> 12) & 0xF) == 0x5 && !((opcode >> 9) & 1)) {
        u32 op = (opcode >> 10) & 0x3;
        u32 ro = THUMB_RN(opcode);
        u32 rb = THUMB_RS(opcode);
        u32 rd = THUMB_RD(opcode);
        u32 addr = cpu->r[rb] + cpu->r[ro];
        
        switch (op) {
            case 0: // STR
                mem_write32(mem, addr & ~3, cpu->r[rd]);
                break;
            case 1: // STRB
                mem_write8(mem, addr, cpu->r[rd] & 0xFF);
                break;
            case 2: // LDR
                cpu->r[rd] = mem_read32(mem, addr & ~3);
                break;
            case 3: // LDRB
                cpu->r[rd] = mem_read8(mem, addr);
                break;
        }
        return 3;
    }
    
    // Load/store sign-extended byte/halfword (0101xx with bit 9=1)
    if (((opcode >> 12) & 0xF) == 0x5 && ((opcode >> 9) & 1)) {
        u32 op = (opcode >> 10) & 0x3;
        u32 ro = THUMB_RN(opcode);
        u32 rb = THUMB_RS(opcode);
        u32 rd = THUMB_RD(opcode);
        u32 addr = cpu->r[rb] + cpu->r[ro];
        
        switch (op) {
            case 0: // STRH
                mem_write16(mem, addr & ~1, cpu->r[rd] & 0xFFFF);
                break;
            case 1: // LDSB
                cpu->r[rd] = (s32)(s8)mem_read8(mem, addr);
                break;
            case 2: // LDRH
                cpu->r[rd] = mem_read16(mem, addr & ~1);
                break;
            case 3: // LDSH
                cpu->r[rd] = (s32)(s16)mem_read16(mem, addr & ~1);
                break;
        }
        return 3;
    }
    
    // Load/store with immediate offset (011xx)
    if (((opcode >> 13) & 0x7) == 0x3) {
        bool load = opcode & (1 << 11);
        bool byte = opcode & (1 << 12);
        u32 offset = THUMB_OFFSET5(opcode);
        u32 rb = THUMB_RS(opcode);
        u32 rd = THUMB_RD(opcode);
        
        if (!byte) offset *= 4;
        u32 addr = cpu->r[rb] + offset;
        
        if (load) {
            if (byte) {
                cpu->r[rd] = mem_read8(mem, addr);
            } else {
                cpu->r[rd] = mem_read32(mem, addr & ~3);
            }
        } else {
            if (byte) {
                mem_write8(mem, addr, cpu->r[rd] & 0xFF);
            } else {
                mem_write32(mem, addr & ~3, cpu->r[rd]);
            }
        }
        return 3;
    }
    
    // Load/store halfword (1000x)
    if (((opcode >> 12) & 0xF) == 0x8) {
        bool load = opcode & (1 << 11);
        u32 offset = ((opcode >> 6) & 0x1F) * 2;
        u32 rb = THUMB_RS(opcode);
        u32 rd = THUMB_RD(opcode);
        u32 addr = cpu->r[rb] + offset;
        
        if (load) {
            cpu->r[rd] = mem_read16(mem, addr & ~1);
        } else {
            mem_write16(mem, addr & ~1, cpu->r[rd] & 0xFFFF);
        }
        return 3;
    }
    
    // SP-relative load/store (1001x)
    if (((opcode >> 12) & 0xF) == 0x9) {
        bool load = opcode & (1 << 11);
        u32 rd = (opcode >> 8) & 0x7;
        u32 offset = (opcode & 0xFF) * 4;
        u32 addr = cpu->r[13] + offset;
        
        if (load) {
            cpu->r[rd] = mem_read32(mem, addr & ~3);
        } else {
            mem_write32(mem, addr & ~3, cpu->r[rd]);
        }
        return 3;
    }
    
    // Load address (1010x)
    if (((opcode >> 12) & 0xF) == 0xA) {
        bool sp = opcode & (1 << 11);
        u32 rd = (opcode >> 8) & 0x7;
        u32 offset = (opcode & 0xFF) * 4;
        
        if (sp) {
            cpu->r[rd] = cpu->r[13] + offset;
        } else {
            cpu->r[rd] = (cpu->r[15] & ~2) + offset;
        }
        return 1;
    }
    
    // Add offset to stack pointer (10110000)
    if ((opcode & 0xFF00) == 0xB000) {
        bool negative = opcode & (1 << 7);
        u32 offset = (opcode & 0x7F) * 4;
        
        if (negative) {
            cpu->r[13] -= offset;
        } else {
            cpu->r[13] += offset;
        }
        return 1;
    }
    
    // Push/pop registers (1011x10x)
    if (((opcode >> 12) & 0xF) == 0xB && ((opcode >> 9) & 0x3) == 0x2) {
        bool load = opcode & (1 << 11);
        bool pc_lr = opcode & (1 << 8);
        u32 rlist = opcode & 0xFF;
        
        if (load) { // POP
            for (int i = 0; i < 8; i++) {
                if (rlist & (1 << i)) {
                    cpu->r[i] = mem_read32(mem, cpu->r[13] & ~3);
                    cpu->r[13] += 4;
                }
            }
            if (pc_lr) {
                cpu->r[15] = mem_read32(mem, cpu->r[13] & ~3);
                cpu->r[13] += 4;
                cpu->r[15] &= ~1;
            }
        } else { // PUSH
            if (pc_lr) {
                cpu->r[13] -= 4;
                mem_write32(mem, cpu->r[13] & ~3, cpu->r[14]);
            }
            for (int i = 7; i >= 0; i--) {
                if (rlist & (1 << i)) {
                    cpu->r[13] -= 4;
                    mem_write32(mem, cpu->r[13] & ~3, cpu->r[i]);
                }
            }
        }
        return 3;
    }
    
    // Multiple load/store (1100x)
    if (((opcode >> 12) & 0xF) == 0xC) {
        bool load = opcode & (1 << 11);
        u32 rb = (opcode >> 8) & 0x7;
        u32 rlist = opcode & 0xFF;
        u32 addr = cpu->r[rb];
        
        for (int i = 0; i < 8; i++) {
            if (rlist & (1 << i)) {
                if (load) {
                    cpu->r[i] = mem_read32(mem, addr & ~3);
                } else {
                    mem_write32(mem, addr & ~3, cpu->r[i]);
                }
                addr += 4;
            }
        }
        
        if (load && !(rlist & (1 << rb))) {
            cpu->r[rb] = addr;
        } else if (!load) {
            cpu->r[rb] = addr;
        }
        
        return 3;
    }
    
    // Conditional branch (1101xxxx, not 1110/1111)
    if (((opcode >> 12) & 0xF) == 0xD && ((opcode >> 8) & 0xF) < 0xE) {
        u32 cond = (opcode >> 8) & 0xF;
        s32 offset = (s32)(s8)(opcode & 0xFF) * 2;
        
        if (check_condition(cpu, cond)) {
            cpu->r[15] = (cpu->r[15] + offset) & 0xFFFFFFFE;
            return 3;
        }
        return 1;
    }
    
    // Software interrupt (11011111)
    if ((opcode & 0xFF00) == 0xDF00) {
        // SWI - similar to ARM SWI handling
        u32 comment = opcode & 0xFF;
        // Basic BIOS call handling
        return 3;
    }
    
    // Unconditional branch (11100)
    if (((opcode >> 11) & 0x1F) == 0x1C) {
        s32 offset = (s32)(THUMB_OFFSET11(opcode) << 21) >> 20; // Sign extend and *2
        cpu->r[15] = (cpu->r[15] + offset) & 0xFFFFFFFE;
        return 3;
    }
    
    // Long branch with link (1111x)
    if (((opcode >> 12) & 0xF) == 0xF) {
        bool second_instruction = opcode & (1 << 11);
        u32 offset = opcode & 0x7FF;
        
        if (!second_instruction) {
            // First instruction: LR = PC + (offset << 12)
            s32 sign_extended = (s32)(offset << 21) >> 9; // Sign extend
            cpu->r[14] = cpu->r[15] + sign_extended;
        } else {
            // Second instruction: PC = LR + (offset << 1), LR = old PC | 1
            u32 temp = cpu->r[15] - 2;
            cpu->r[15] = (cpu->r[14] + (offset << 1)) & 0xFFFFFFFE;
            cpu->r[14] = temp | 1;
        }
        return 3;
    }
    
    // Unimplemented Thumb instruction
    return 1;
}

u32 cpu_step(ARM7TDMI *cpu, Memory *mem) {
    if (cpu->halted) return 1;
    
    u32 pc = cpu->r[15];
    
    if (cpu->thumb_mode) {
        u16 opcode = mem_read16(mem, pc);
        cpu->r[15] += 2;
        return execute_thumb(cpu, mem, opcode);
    } else {
        u32 opcode = mem_read32(mem, pc);
        cpu->r[15] += 4;
        return execute_arm(cpu, mem, opcode);
    }
}

void cpu_execute_frame(ARM7TDMI *cpu, Memory *mem) {
    // GBA runs at ~16.78 MHz
    // At 60 FPS, that's ~280,000 cycles per frame
    const u32 CYCLES_PER_FRAME = 280000;
    
    u32 cycles_executed = 0;
    while (cycles_executed < CYCLES_PER_FRAME && !cpu->halted) {
        u32 cycles = cpu_step(cpu, mem);
        cycles_executed += cycles;
        cpu->cycles += cycles;
    }
}
