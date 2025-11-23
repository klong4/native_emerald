#include "cpu_core.h"
#include "memory.h"
#include "interrupts.h"
#include "debug_trace.h"
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
    
    // Skip BIOS and jump directly to ROM for now
    // BIOS boot has some issues that need more investigation
    // Note: PC (R15) is always ahead by +8 (ARM) or +4 (Thumb)
    cpu->r[15] = 0x08000000 + 8;  // PC - ROM entry point + pipeline offset
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
    
    // Branch and exchange (BX) - MUST be checked before MSR/data processing!
    // Pattern: 0xE12FFF1x where x is the register
    if ((opcode & 0x0FFFFFF0) == 0x012FFF10) {
        u32 rn = opcode & 0xF;
        // When rn is PC, it reads as current instruction + 8
        // R15 is PC+12 after increment, subtract 4 to get PC+8
        u32 addr = (rn == 15) ? (cpu->r[15] - 4) : cpu->r[rn];
        
        // Log when game tries to branch to BIOS (address 0-0x1C)
        // This indicates a reset or exception - we want to know why
        if (addr <= 0x1C) {
            static int bx_zero_count = 0;
            if (bx_zero_count < 10) {
                printf("\n[BX→BIOS #%d] PC=0x%08X: BX R%d (value=0x%08X) - Function returned NULL!\n",
                       bx_zero_count, cpu->r[15] - 4, rn, addr);
                printf("  Return registers: R0=%08X R1=%08X R2=%08X R3=%08X\n",
                       cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3]);
                printf("  Link Register: LR=0x%08X (return address from last BL/BLX)\n", cpu->r[14]);
                printf("  Stack Pointer: SP=0x%08X, CPSR=%08X\n", cpu->r[13], cpu->cpsr);
                printf("  This suggests the called function at 0x%08X failed and returned NULL\n",
                       (cpu->r[14] & ~1) - 4);
                bx_zero_count++;
            }
        }
        
        // Validate branch target is in valid memory region
        // Valid regions: ROM (0x08000000+), IWRAM (0x03000000+), EWRAM (0x02000000+)
        if (addr >= 0x10000000 || (addr >= 0x04000000 && addr < 0x08000000)) {
            static u32 logged_bx = 0;
            if (logged_bx != cpu->r[15] - 4) {
                printf("[BX] Invalid target 0x%08X from PC=0x%08X, R%d=0x%08X, skipping\n",
                       addr, cpu->r[15] - 4, rn, cpu->r[rn]);
                logged_bx = cpu->r[15] - 4;
            }
            // Don't branch to invalid address - just skip this instruction
            return 3;
        }
        
        // Track BX LR (function returns) to see if functions are completing successfully
        static int bx_lr_count = 0;
        if (rn == 14 && bx_lr_count < 10 && addr > 0x08000000 && addr < 0x09000000) {
            printf("[ARM BX LR] Returning from PC=0x%08X to 0x%08X (Thumb=%d), R0=0x%08X\n",
                   cpu->r[15] - 4, addr, addr & 1, cpu->r[0]);
            bx_lr_count++;
        }
        
        cpu->thumb_mode = addr & 1;
        u32 target = addr & 0xFFFFFFFE;
        // Set R15 to maintain pipeline invariant: R15 = PC + (thumb ? 4 : 8)
        cpu->r[15] = target + (cpu->thumb_mode ? 4 : 8);
        
        return 3;
    }
    
    // MRS - Move PSR to register (must be checked before data processing)
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
    
    // MSR - Move register to PSR (must be checked before data processing)
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
            u32 new_cpsr = (cpu->cpsr & ~field_mask) | (value & field_mask);
            
            // Validate mode bits if control field is being modified
            if (field_mask & 0xFF) {
                u32 new_mode = new_cpsr & 0x1F;
                // Valid ARM modes: 0x10=User, 0x11=FIQ, 0x12=IRQ, 0x13=Supervisor,
                // 0x17=Abort, 0x1B=Undefined, 0x1F=System
                if (new_mode != 0x10 && new_mode != 0x11 && new_mode != 0x12 && 
                    new_mode != 0x13 && new_mode != 0x17 && new_mode != 0x1B && new_mode != 0x1F) {
                    // Invalid mode - preserve current mode bits
                    new_cpsr = (new_cpsr & ~0x1F) | (cpu->cpsr & 0x1F);
                }
            }
            
            cpu->cpsr = new_cpsr;
            // Update thumb_mode flag if bit 5 changed
            // NOTE: Bit 5 (0x20) is the Thumb state bit in CPSR
            cpu->thumb_mode = (cpu->cpsr & (1 << 5)) != 0;
        }
        return 1;
    }
    
    // Multiply (000x with bits 4-7 = 1001)
    if ((opcode & 0x0FC000F0) == 0x00000090) {
        bool accumulate = opcode & (1 << 21);
        bool set_flags = opcode & (1 << 20);
        u32 rd = ARM_RD(opcode);
        u32 rn = ARM_RN(opcode);
        u32 rs = ARM_RS(opcode);
        u32 rm = ARM_RM(opcode);
        
        // Note: PC not typically used in multiply, but handle it correctly anyway
        // R15 is PC+12 after increment, subtract 4 to get PC+8
        u32 rm_val = (rm == 15) ? (cpu->r[15] - 4) : cpu->r[rm];
        u32 rs_val = (rs == 15) ? (cpu->r[15] - 4) : cpu->r[rs];
        u32 rn_val = (rn == 15) ? (cpu->r[15] - 4) : cpu->r[rn];
        
        u32 result = rm_val * rs_val;
        if (accumulate) result += rn_val;
        
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
        
        // When rn/rm is PC, it reads as current instruction + 8
        // R15 is PC+12 after increment, subtract 4 to get PC+8
        u32 addr = (rn == 15) ? (cpu->r[15] - 4) : cpu->r[rn];
        u32 rm_val = (rm == 15) ? (cpu->r[15] - 4) : cpu->r[rm];
        if (byte) {
            u32 temp = mem_read8(mem, addr);
            mem_write8(mem, addr, rm_val & 0xFF);
            cpu->r[rd] = temp;
        } else {
            u32 temp = mem_read32(mem, addr & ~3);
            mem_write32(mem, addr & ~3, rm_val);
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
            
            // When rm is r15 (PC), it reads as current instruction + 8
            // R15 is PC+12 after increment, subtract 4 to get PC+8
            u32 rm_val = (rm == 15) ? (cpu->r[15] - 4) : cpu->r[rm];
            operand2 = barrel_shift(cpu, rm_val, shift_type, shift_amount, 
                                   set_flags && (opcode_type & 0xC) != 0x8);
        }
        
        // When rn is r15 (PC), it reads as current instruction + 8
        // R15 is PC+12 after increment, subtract 4 to get PC+8
        u32 op1 = (rn == 15) ? (cpu->r[15] - 4) : cpu->r[rn];
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
            // When writing to PC with S flag set, restore CPSR from SPSR
            // This is used for exception returns (e.g., SUBS PC, LR, #4)
            if (set_flags) {
                cpu->cpsr = cpu->spsr;
                cpu->thumb_mode = (cpu->cpsr & (1 << 5)) != 0;
            }
            
            u32 new_pc = result & 0xFFFFFFFE;
            // Validate PC target is in valid memory region
            if (new_pc >= 0x10000000 || (new_pc >= 0x04000000 && new_pc < 0x08000000)) {
                static u32 logged_mov_pc = 0;
                if (logged_mov_pc != cpu->r[15] - 4) {
                    printf("[MOV PC] Invalid target 0x%08X from PC=0x%08X, skipping\n",
                           new_pc, cpu->r[15] - 4);
                    logged_mov_pc = cpu->r[15] - 4;
                }
                // Don't modify PC to invalid address
            } else {
                cpu->r[15] = new_pc;
                if (!set_flags) { // Only set thumb mode from bit 0 if NOT restoring CPSR
                    cpu->thumb_mode = result & 1;
                }
            }
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
            // When rm is PC, it reads as current instruction + 8
            // R15 is PC+12 after increment, subtract 4 to get PC+8
            u32 rm_val = (rm == 15) ? (cpu->r[15] - 4) : cpu->r[rm];
            offset = barrel_shift(cpu, rm_val, shift_type, shift_amount, false);
        }
        
        // When rn is PC, it reads as current instruction + 8
        // R15 is PC+12 after increment, subtract 4 to get PC+8
        u32 addr = (rn == 15) ? (cpu->r[15] - 4) : cpu->r[rn];
        
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
            // If loading into PC, handle mode switching
            if (rd == 15) {
                u32 new_pc = cpu->r[15] & 0xFFFFFFFE;
                // Validate PC target is in valid memory region
                if (new_pc >= 0x10000000 || (new_pc >= 0x04000000 && new_pc < 0x08000000)) {
                    static u32 logged_ldr_pc = 0;
                    if (logged_ldr_pc != (cpu->r[15] - 4)) {
                        printf("[LDR PC] Invalid target 0x%08X from PC=0x%08X, addr=0x%08X, skipping\n",
                               new_pc, cpu->r[15] - 4, addr);
                        logged_ldr_pc = cpu->r[15] - 4;
                    }
                    // Reset PC to next instruction instead
                    cpu->r[15] = (cpu->r[15] - 4) + 4;  // Just continue
                } else {
                    cpu->thumb_mode = cpu->r[15] & 1;
                    cpu->r[15] = new_pc;
                }
            }
        } else {
            // When rd is PC for store, it stores PC+12 (R15 is PC+8, so +4 more)
            u32 store_val = (rd == 15) ? (cpu->r[15] + 4) : cpu->r[rd];
            if (byte) {
                mem_write8(mem, addr, store_val & 0xFF);
            } else {
                mem_write32(mem, addr & ~3, store_val);
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
        
        // When rn is PC, it reads as current instruction + 8
        // R15 is PC+12 after increment, subtract 4 to get PC+8
        u32 addr = (rn == 15) ? (cpu->r[15] - 4) : cpu->r[rn];
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
                    // When storing PC, it stores PC+12 (R15 is PC+8, so +4 more)
                    u32 store_val = (i == 15) ? (cpu->r[15] + 4) : cpu->r[i];
                    mem_write32(mem, addr & ~3, store_val);
                }
                
                if (!pre_index) addr += 4;
            }
        }
        
        // If PC was loaded, handle mode switching
        if (load && (rlist & (1 << 15))) {
            // If load_psr (S bit) is set and we're loading PC:
            // - In privileged modes (not User/System): restore CPSR from SPSR (exception return)
            // - In User/System modes: load user registers (doesn't affect CPSR)
            u32 current_mode = cpu->cpsr & 0x1F;
            bool is_privileged = (current_mode != 0x10 && current_mode != 0x1F);
            
            if (load_psr && is_privileged) {
                // Exception return: restore CPSR from SPSR
                cpu->cpsr = cpu->spsr;
                cpu->thumb_mode = (cpu->cpsr & (1 << 5)) != 0;
            } else {
                // Normal return or user-mode LDM: use PC bit 0 for mode
                cpu->thumb_mode = cpu->r[15] & 1;
            }
            cpu->r[15] = cpu->r[15] & 0xFFFFFFFE;
        }
        
        if (writeback) {
            if (up) cpu->r[rn] = start_addr + count * 4;
            else cpu->r[rn] = start_addr;
        }
        
        return count + 2;
    }
    
    // Branch (101)
    if (op_type == 0x5) {
        bool link = opcode & (1 << 24);
        s32 offset = (s32)(ARM_OFFSET24(opcode) << 8) >> 6; // Sign extend and *4
        
        if (link) {
            // LR = address of next instruction = (current PC) + 4
            // R15 is currently PC+12, so (R15-12)+4 = R15-8
            cpu->r[14] = cpu->r[15] - 8;
        }
        
        // Branch: offset is relative to PC+8
        // R15 is currently PC+12 (after increment in cpu_step)
        // So PC+8 = R15-4
        u32 pc_plus_8 = cpu->r[15] - 4;
        u32 target = (pc_plus_8 + offset) & 0xFFFFFFFE;
        
        // Set R15 to target+8 to maintain PC+8 invariant
        cpu->r[15] = target + 8;
        
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
            // When rm is PC, it reads as current instruction + 8
            // R15 is PC+12 after increment, subtract 4 to get PC+8
            offset = (rm == 15) ? (cpu->r[15] - 4) : cpu->r[rm];
        }
        
        // When rn is PC, it reads as current instruction + 8
        // R15 is PC+12 after increment, subtract 4 to get PC+8
        u32 addr = (rn == 15) ? (cpu->r[15] - 4) : cpu->r[rn];
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
            // If loading into PC, handle mode switching
            if (rd == 15) {
                cpu->thumb_mode = cpu->r[rd] & 1;
                cpu->r[15] = cpu->r[15] & 0xFFFFFFFE;
            }
        } else {
            if (sh == 1) { // STRH
                // When rd is PC, it stores PC+12 (R15 is PC+8, so +4 more)
                u32 store_val = (rd == 15) ? (cpu->r[15] + 4) : cpu->r[rd];
                mem_write16(mem, addr & ~1, store_val & 0xFFFF);
            }
        }
        
        if (!pre_index) {
            if (up) cpu->r[rn] += offset;
            else cpu->r[rn] -= offset;
        } else if (writeback && rn != rd) {  // No writeback if rn == rd for loads
            cpu->r[rn] = addr;
        }
        
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
                
            case 0x01: // RegisterRamReset
                {
                    // R0 contains flags for what to reset
                    u32 flags = cpu->r[0];
                    printf("[BIOS] RegisterRamReset called with flags=0x%02X\n", flags);
                    // Bit 0: Clear 256KB EWRAM
                    // Bit 1: Clear 32KB IWRAM (excluding last 0x200 bytes for stack)
                    // Bit 2: Clear Palette RAM
                    // Bit 3: Clear VRAM
                    // Bit 4: Clear OAM
                    // Bit 5: Reset SIO registers
                    // Bit 6: Reset Sound registers
                    // Bit 7: Reset all other registers
                    // For now just acknowledge - memory should already be zero-initialized
                }
                break;
                
            case 0x02: // Halt
                cpu->halted = true;
                break;
                
            case 0x03: // Stop
                cpu->halted = true;
                break;
                
            case 0x04: // IntrWait
                // r0 = discard old flags (0=check, 1=discard)
                // r1 = interrupt mask
                // Halt CPU until interrupt in mask occurs
                cpu->halted = true;
                break;
                
            case 0x05: // VBlankIntrWait
                // Wait for VBlank interrupt
                cpu->halted = true;
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
                
            case 0x0E: // BgAffineSet
                // R0 = source, R1 = dest, R2 = count
                // Affine transformation for backgrounds
                // Just acknowledge for now
                break;
                
            case 0x0F: // ObjAffineSet  
                // R0 = source, R1 = dest, R2 = count, R3 = offset
                // Affine transformation for sprites
                // Just acknowledge for now
                break;
                
            case 0x13: // HuffUnComp
                // Huffman decompression - rarely used
                // Just acknowledge for now
                break;
                
            case 0x16: // Diff8bitUnFilterWram
            case 0x17: // Diff8bitUnFilterVram
            case 0x18: // Diff16bitUnFilter
                // Differential filter decompression
                // Just acknowledge for now
                break;
                
            case 0x19: // SoundBias
                // Sound bias control
                break;
                
            case 0x1F: // MidiKey2Freq
                // MIDI key to frequency conversion
                // Just acknowledge
                break;
                
            case 0x28: // SoundDriverVSyncOff
            case 0x29: // SoundDriverVSyncOn  
                // Sound driver vsync control
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
    if (((opcode >> 13) & 0x7) == 0x0) {
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
                {
                    u32 result = cpu->r[rd] + cpu->r[rs];
                    if (rd == 15) {
                        // Adding to PC - extract Thumb bit and add pipeline offset
                        cpu->thumb_mode = result & 1;
                        u32 target = result & 0xFFFFFFFE;
                        cpu->r[15] = target + (cpu->thumb_mode ? 4 : 8);
                    } else {
                        cpu->r[rd] = result;
                    }
                }
                break;
            case 1: // CMP
                {
                    u32 result = cpu->r[rd] - cpu->r[rs];
                    update_flags_sub(cpu, cpu->r[rd], cpu->r[rs], result);
                }
                break;
            case 2: // MOV
                if (rd == 15) {
                    // Moving to PC - extract Thumb bit and add pipeline offset
                    cpu->thumb_mode = cpu->r[rs] & 1;
                    u32 target = cpu->r[rs] & 0xFFFFFFFE;
                    cpu->r[15] = target + (cpu->thumb_mode ? 4 : 8);
                } else {
                    cpu->r[rd] = cpu->r[rs];
                }
                break;
            case 3: // BX/BLX
                {
                    u32 addr = cpu->r[rs];
                    cpu->thumb_mode = addr & 1;
                    u32 target = addr & 0xFFFFFFFE;
                    // Set R15 to maintain pipeline invariant: R15 = PC + (thumb ? 4 : 8)
                    cpu->r[15] = target + (cpu->thumb_mode ? 4 : 8);
                }
                return 3;
        }
        return 1;
    }
    
    // PC-relative load (01001)
    if (((opcode >> 11) & 0x1F) == 0x9) {
        u32 rd = (opcode >> 8) & 0x7;
        u32 offset = (opcode & 0xFF) << 2;
        // PC should be current instruction + 4, but R15 is incremented to PC+6, so subtract 2
        u32 addr = ((cpu->r[15] - 2) & ~3) + offset;
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
            // PC should be current instruction + 4, but R15 is incremented to PC+6, so subtract 2
            cpu->r[rd] = ((cpu->r[15] - 2) & ~3) + offset;
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
                u32 addr = mem_read32(mem, cpu->r[13] & ~3);
                cpu->r[13] += 4;
                // Extract Thumb mode from bit 0 of the popped address
                cpu->thumb_mode = addr & 1;
                // Mask off Thumb bit and add pipeline offset
                u32 target = addr & 0xFFFFFFFE;
                cpu->r[15] = target + (cpu->thumb_mode ? 4 : 8);
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
            // R15 is at PC+6 after pipeline increment (instruction+4+2)
            // Branch offset is relative to PC+4 (instruction address + 4)
            // Calculate target instruction address: (R15 - 2) + offset = (instr+4+2-2) + offset = (instr+4) + offset
            // But we need to set R15 for next fetch, which expects R15-4 = target instruction
            // So set R15 = target + 4
            u32 target_addr = ((cpu->r[15] - 2) + offset) & 0xFFFFFFFE;
            cpu->r[15] = target_addr + 4;
            return 3;
        }
        return 1;
    }
    
    // Software interrupt (11011111)
    if ((opcode & 0xFF00) == 0xDF00) {
        // SWI in Thumb mode - handle same as ARM SWI
        u32 comment = opcode & 0xFF;
        
        // Call ARM SWI handler (it's implemented in ARM mode section)
        // For now, handle common SWIs here
        switch (comment) {
            case 0x02: // Halt
            case 0x03: // Stop
                cpu->halted = true;
                break;
            case 0x04: // IntrWait
            case 0x05: // VBlankIntrWait
                cpu->halted = true;
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
            case 0x0C: // CpuFastSet
                // Memory copy/fill operations
                // R0 = source, R1 = dest, R2 = length/mode
                // For now, just acknowledge
                break;
            case 0x0D: // GetBiosChecksum
                cpu->r[0] = 0xBAAE187F; // Correct BIOS checksum
                break;
            default:
                // Unknown SWI
                break;
        }
        
        return 3;
    }
    
    // Unconditional branch (11100)
    if (((opcode >> 11) & 0x1F) == 0x1C) {
        s32 offset = (s32)(THUMB_OFFSET11(opcode) << 21) >> 20; // Sign extend and *2
        cpu->r[15] = (cpu->r[15] + offset) & 0xFFFFFFFE;
        return 3;
    }
    
    // Long branch with link (1111x) - 32-bit instruction
    if (((opcode >> 12) & 0xF) == 0xF) {
        bool second_instruction = opcode & (1 << 11);
        
        if (!second_instruction) {
            // First half of BL/BLX - fetch second half and execute as single instruction
            u32 offset_high = opcode & 0x7FF;
            s32 sign_extended = (s32)(offset_high << 21) >> 9; // Sign extend and shift
            
            // Fetch second half of instruction
            // At this point: R15 has been incremented by 2 (in cpu_step)
            // Original PC was: current_R15 - 2
            // Current instruction (first half) is at: (original_PC) - 4 = R15 - 6
            // Second half is at: current_instruction + 2 = R15 - 4
            u16 second_half = mem_read16(mem, cpu->r[15] - 4);
            cpu->r[15] += 2; // Advance PC past second half (now at next instruction + 4)
            
            // Check if this is BL (1111 1xxx) or BLX (1110 1xxx)
            // BLX: bits 12-15 = 1110 (switches to ARM mode)
            // BL:  bits 12-15 = 1111 (stays in Thumb mode)
            bool is_blx = ((second_half >> 12) == 0xE); // Exactly 1110, not 1111
            bool is_bl = ((second_half >> 12) == 0xF);   // Exactly 1111
            
            if ((is_bl || is_blx) && (second_half & (1 << 11))) {
                u32 offset_low = second_half & 0x7FF;
                
                // Calculate target address
                // R15 is now at: (instruction_addr + 4) + 4 = instruction_addr + 8
                // BL offset is relative to (instruction_addr + 4)
                // So target = (R15 - 4) + offset
                u32 base_pc = cpu->r[15] - 4;
                u32 target = base_pc + sign_extended + (offset_low << 1);
                
                // For BLX, add the H bit (bit 0 of target is determined by bit 0 of instruction)
                if (is_blx) {
                    // BLX switches to ARM mode - bit 1 of offset determines final bit
                    target = (target & 0xFFFFFFFC) | ((second_half & 1) << 1);
                }
                
                static int bl_count = 0;
                static u32 last_blx_target = 0;
                u32 instr_addr = cpu->r[15] - 8;
                
                // Log BLX instructions and problematic BL calls around 0x3C6
                if (is_blx && (bl_count < 15 || target != last_blx_target)) {
                    printf("[THUMB BLX #%d] PC=0x%08X → target=0x%08X (second_half=0x%04X, is_blx=%d), LR=0x%08X\n",
                           bl_count, cpu->r[15] - 4, target, second_half, is_blx, cpu->r[15] | 1);
                    last_blx_target = target;
                    bl_count++;
                }
                
                // BL logging disabled - enable for debugging if needed
                bl_count++;
                
                // Save return address in LR with bit 0 set (return to Thumb)
                // R15 is now at: (current_instruction + 4) + 4 = current_instruction + 8
                // Return address is: current_instruction + 4 (next instruction after this BL)
                // So LR = (R15 - 4) | 1
                cpu->r[14] = (cpu->r[15] - 4) | 1;
                
                // Branch to target
                if (is_blx) {
                    // BLX switches to ARM mode
                    cpu->thumb_mode = false;
                    cpu->r[15] = target & 0xFFFFFFFC;  // ARM addresses must be word-aligned
                } else {
                    // BL stays in Thumb mode
                    cpu->thumb_mode = true;
                    cpu->r[15] = target & 0xFFFFFFFE;
                }
                
                return 3;
            } else {
                // Invalid BL/BLX sequence
                return 1;
            }
        } else {
            // Orphaned second half
            return 1;
        }
    }
    
    // Unimplemented Thumb instruction
    return 1;
}

u32 cpu_step(ARM7TDMI *cpu, Memory *mem) {
    if (cpu->halted) return 1;
    
    u32 pc = cpu->r[15];
    
    // Detailed trace of stuck loop - DISABLED for performance
    /*
    static int loop_trace_count = 0;
    u32 actual_pc = cpu->thumb_mode ? (pc - 4) : (pc - 8);
    if (loop_trace_count < 200) {
        u32 instr = cpu->thumb_mode ? mem_read16(mem, actual_pc) : mem_read32(mem, actual_pc);
        printf("[TRACE %3d] PC=0x%08X %s I=0x%04X | R0=%08X R1=%08X R14=%08X SP=%08X\n",
               loop_trace_count++, actual_pc, cpu->thumb_mode ? "T" : "A", instr,
               cpu->r[0], cpu->r[1], cpu->r[14], cpu->r[13]);
    }
    */
    
    // Boot sequence - log first few instructions if needed for debugging
    static int boot_instr_count = 0;
    
    // Trace main loop to understand what game is doing
    static int main_loop_trace = 0;
    static int trace_after_delay = 0;
    static u64 global_instr_count = 0;
    global_instr_count++;
    
    // Boot tracing disabled - enable for debugging if needed
    /*
    if (boot_instr_count < 50) {
        u32 instr_addr = cpu->thumb_mode ? (pc - 4) : (pc - 8);
        if ((cpu->thumb_mode && pc >= 4) || (!cpu->thumb_mode && pc >= 8)) {
            u32 instr = cpu->thumb_mode ? mem_read16(mem, instr_addr) : mem_read32(mem, instr_addr);
            printf("[BOOT %2d] R15=0x%08X | %s | I=0x%08X | R0=%08X R14=%08X\n",
                   boot_instr_count, pc, cpu->thumb_mode ? "T" : "A", instr,
                   cpu->r[0], cpu->r[14]);
        }
        boot_instr_count++;
    }
    */
    
    // Tracing disabled for performance
    /*
    // Skip early boot and trace specific loop area
    if (trace_after_delay > 1000 && main_loop_trace < 200) {
        u32 instr_addr = cpu->thumb_mode ? (pc - 4) : (pc - 8);
        if ((cpu->thumb_mode && pc >= 4) || (!cpu->thumb_mode && pc >= 8)) {
            u32 instr = cpu->thumb_mode ? mem_read16(mem, instr_addr) : mem_read32(mem, instr_addr);
            printf("[TRACE %3d] PC=0x%08X | %s | I=0x%08X | R0=%08X R1=%08X R14=%08X | CPSR=%08X\n",
                   main_loop_trace, pc, cpu->thumb_mode ? "T" : "A", instr,
                   cpu->r[0], cpu->r[1], cpu->r[14], cpu->cpsr);
            main_loop_trace++;
        }
    } else {
        trace_after_delay++;
    }
    */
    
    if (boot_instr_count < 0) {
        // Calculate instruction address
        u32 instr_addr = cpu->thumb_mode ? (pc - 4) : (pc - 8);
        
        // Safety check: only read instruction if address is valid
        // Skip trace for PC < 8/4 (would cause wraparound) but don't reset PC
        if ((cpu->thumb_mode && pc >= 4) || (!cpu->thumb_mode && pc >= 8)) {
            u32 instr = cpu->thumb_mode ? mem_read16(mem, instr_addr) : mem_read32(mem, instr_addr);
            printf("[BOOT %4d] PC=0x%08X | %s | I=0x%08X | R0=%08X R1=%08X R13=%08X R14=%08X\n",
                   boot_instr_count, pc, cpu->thumb_mode ? "T" : "A", instr,
                   cpu->r[0], cpu->r[1], cpu->r[13], cpu->r[14]);
        }
        boot_instr_count++;
        if (boot_instr_count == 100) {
            printf("\n[BOOT] First 100 instructions complete, disabling trace\n");
            printf("[BOOT] global_instr_count = %llu\n\n", global_instr_count);
        }
    }
    
    // Debug: detect stuck in specific loop
    static u32 last_pc_check = 0;
    static int pc_stuck_count = 0;
    static int trace_stuck_loop = 0;
    
    // Detect if we're stuck at 0x082DFAF4 specifically (compiled ROM)
    // OR at 0x08000496 (original ROM)
    // Instruction is: CMP R4, #0; BNE -42  (loops while R4 != 0)
    // The loop spans 0x082DFACA to 0x082DFAF4 (compiled ROM)
    // OR 0x08000470 to 0x08000496 (original ROM - estimated)
    static int consecutive_in_loop = 0;
    bool in_compiled_loop = (pc >= 0x082DFACA && pc <= 0x082DFAF6);
    bool in_original_loop = (pc >= 0x08000470 && pc <= 0x080004A0);
    
    if (in_compiled_loop || in_original_loop) {
        consecutive_in_loop++;
        
        if (pc == 0x082DFAF4 || pc == 0x08000496) {
            pc_stuck_count++;
            if (pc_stuck_count <= 5 || (pc_stuck_count % 100000) == 0) {
                printf("[DEBUG] Loop at 0x%08X, count=%d, consecutive=%d, R4=0x%08X (waiting for R4 == 0)\n", 
                       pc, pc_stuck_count, consecutive_in_loop, cpu->r[4]);
            }
            if (consecutive_in_loop >= 3 && trace_stuck_loop == 0) {
                printf("\n[STUCK LOOP] In loop for %d consecutive instructions at PC=0x%08X\n", consecutive_in_loop, pc);
                printf("  R4=0x%08X (waiting for R4 to become ZERO)\n", cpu->r[4]);
                printf("  R5=0x%08X\n", cpu->r[5]);
                printf("  Will trace next 100 instructions...\n");
                trace_stuck_loop = 100;
            }
        }
        
        // Trace when we're about to LOAD R4 (the instruction before the CMP in compiled ROM)
        if (pc == 0x082DFAF0 || pc == 0x08000492) {
            static int load_r4_count = 0;
            if (load_r4_count < 10) {
                // About to execute: LDR R4, [R4, #0x34] or similar
                u32 base_addr = cpu->r[4];
                printf("[R4 LOAD #%d] PC=0x%08X: R4=#0x%08X, R5=0x%08X\n",
                       load_r4_count, pc, base_addr, cpu->r[5]);
                load_r4_count++;
            }
        }
    } else {
        consecutive_in_loop = 0;
        pc_stuck_count = 0;
    }
    
    if (trace_stuck_loop > 0) {
        if (cpu->thumb_mode && pc >= 4) {
            u16 opcode = mem_read16(mem, pc - 4);
            u32 z = (cpu->cpsr & FLAG_Z) ? 1 : 0;
            u32 n = (cpu->cpsr & FLAG_N) ? 1 : 0;
            u32 v = (cpu->cpsr & FLAG_V) ? 1 : 0;
            u32 c = (cpu->cpsr & FLAG_C) ? 1 : 0;
            
            // Highlight R4 changes
            static u32 last_r4 = 0;
            const char* r4_marker = "";
            if (cpu->r[4] != last_r4) {
                r4_marker = " <-- R4 CHANGED!";
                last_r4 = cpu->r[4];
            }
            
            printf("[TRACE #%03d] PC=0x%08X opcode=0x%04X | R4=%08X R5=%08X | Z=%d N=%d%s\n",
                   100 - trace_stuck_loop, pc - 4, opcode,
                   cpu->r[4], cpu->r[5],
                   z, n, r4_marker);
        }
        trace_stuck_loop--;
    }
    
    // Check for misaligned PC in Thumb mode (critical bug detector)
    if (cpu->thumb_mode && (pc & 1)) {
        static int misalign_count = 0;
        if (misalign_count < 5) {
            printf("\n[CRITICAL] Misaligned PC in Thumb mode!\n");
            printf("  PC=0x%08X (ODD address - should be EVEN in Thumb mode)\n", pc);
            printf("  This will cause prefetch abort\n");
            printf("  LR=0x%08X, CPSR=0x%08X\n", cpu->r[14], cpu->cpsr);
            printf("  R0-R3: %08X %08X %08X %08X\n", cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3]);
            misalign_count++;
        }
        // Fix it by masking off bit 0
        cpu->r[15] = pc & 0xFFFFFFFE;
        pc = cpu->r[15];
    }
    
    // Validate CPSR mode bits (should be one of the valid ARM modes)
    u32 mode = cpu->cpsr & 0x1F;
    if (mode != 0x10 && mode != 0x11 && mode != 0x12 && mode != 0x13 && 
        mode != 0x17 && mode != 0x1B && mode != 0x1F) {
        // CPSR corrupted - reset to system mode
        static int cpsr_log_count = 0;
        if (cpsr_log_count < 3) {
            printf("[CPSR CORRUPTION] Invalid mode 0x%02X in CPSR=0x%08X at PC=0x%08X\n", 
                   mode, cpu->cpsr, pc);
            printf("  R0-R3:  %08X %08X %08X %08X\n", cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3]);
            printf("  R12-15: %08X %08X %08X %08X\n", cpu->r[12], cpu->r[13], cpu->r[14], cpu->r[15]);
            printf("  SPSR:   %08X\n", cpu->spsr);
            cpsr_log_count++;
        }
        cpu->cpsr = (cpu->cpsr & 0xFFFFFFE0) | 0x1F;  // System mode
    }
    
    // Validate PC is in a valid memory region
    // Valid: BIOS (0x0-0x3FFF), EWRAM (0x02000000+), IWRAM (0x03000000+), ROM (0x08000000+)
    bool valid_pc = (pc < 0x4000) || 
                    (pc >= 0x02000000 && pc < 0x04000000) ||
                    (pc >= 0x08000000 && pc < 0x0E000000);
    
    if (!valid_pc) {
        // PC is in invalid region
        static u32 last_bad_pc = 0;
        if (pc != last_bad_pc) {
            printf("[PC CORRUPTION] Invalid PC=0x%08X, LR=0x%08X, resetting to ROM entry\n", 
                   pc, cpu->r[14]);
            printf("  R0-R3:  %08X %08X %08X %08X\n", cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3]);
            printf("  R12-R15: %08X %08X %08X %08X\n", cpu->r[12], cpu->r[13], cpu->r[14], cpu->r[15]);
            printf("  CPSR: %08X, Mode: %s\n", cpu->cpsr, cpu->thumb_mode ? "Thumb" : "ARM");
            last_bad_pc = pc;
        }
        cpu->r[15] = 0x08000000;
        cpu->thumb_mode = false;
        return 3;
    }
    
    // BIOS call detection and HLE
    // If PC is in BIOS range (0x0000-0x3FFF) and not at an exception vector
    if (pc < 0x4000 && pc >= 0x20) {
        // Game is trying to execute BIOS code directly
        // This happens when games do BX to BIOS addresses or exceptions occur
        
        // Exception vectors that might be hit:
        // 0x00: Reset, 0x04: Undefined, 0x08: SWI, 0x0C: Prefetch abort,
        // 0x10: Data abort, 0x18: IRQ, 0x1C: FIQ
        
        if (pc >= 0x04 && pc < 0x20) {
            // Exception occurred - log it once
            static u32 last_exception_pc = 0;
            if (pc != last_exception_pc) {
                const char *exc_name[] = {"UND", "SWI", "PRE", "DAT", "RES", "RES", "IRQ", "FIQ"};
                printf("[EXCEPTION] %s at vector 0x%02X, LR=0x%08X, CPSR=0x%08X\n", 
                       exc_name[(pc-4)/4], pc, cpu->r[14], cpu->cpsr);
                last_exception_pc = pc;
            }
        }
        
        // We handle this by returning to the caller (in LR)
        if (cpu->r[14] >= 0x08000000) {
            // Valid return address in ROM - return there
            cpu->thumb_mode = cpu->r[14] & 1;
            u32 target = cpu->r[14] & 0xFFFFFFFE;
            cpu->r[15] = target + (cpu->thumb_mode ? 4 : 8);
            return 3;
        } else {
            // No valid return address - jump to ROM entry point
            cpu->r[15] = 0x08000000 + 8;  // ARM mode, so PC = target + 8
            cpu->thumb_mode = false;
            return 3;
        }
    }
    
    // Debug tracing if enabled
    if (debug_should_trace(pc)) {
        // Safety check for wraparound
        if ((cpu->thumb_mode && pc >= 4) || (!cpu->thumb_mode && pc >= 8)) {
            if (cpu->thumb_mode) {
                u16 opcode = mem_read16(mem, pc - 4);
                debug_trace_instruction(pc - 4, opcode, true, "");
            } else {
                u32 opcode = mem_read32(mem, pc - 8);
                debug_trace_instruction(pc - 8, opcode, false, "");
            }
        }
    }
    
    // Special handling for PC at BIOS exception vectors (0x00-0x1C)
    // Log when game tries to jump to BIOS vectors to understand why
    if (pc >= 0 && pc <= 0x1C) {
        static int bx_to_bios_count = 0;
        static u32 last_vector_pc = 0xFFFFFFFF;
        if (pc != last_vector_pc || bx_to_bios_count < 10) {
            const char* vector_name = "Unknown";
            if (pc == 0x00) vector_name = "Reset";
            else if (pc == 0x04) vector_name = "Undefined Instruction";
            else if (pc == 0x08) vector_name = "Prefetch Abort";
            else if (pc == 0x0C) vector_name = "Data Abort";
            else if (pc == 0x18) vector_name = "IRQ";
            
            printf("\n[BIOS VECTOR 0x%02X - %s] Exception occurred!\n", (unsigned int)pc, vector_name);
            printf("  Previous PC was: 0x%08X\n", pc - (cpu->thumb_mode ? 4 : 8));
            printf("  Return address: LR=0x%08X (instruction that caused exception)\n", cpu->r[14]);
            printf("  CPSR=0x%08X, Mode=%s, Thumb=%d\n",
                   cpu->cpsr, cpu->thumb_mode ? "T" : "A", cpu->thumb_mode);
            printf("  R0-R3: %08X %08X %08X %08X\n", cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3]);
            
            // For prefetch abort, LR points to the instruction that failed + offset
            if (pc == 0x08 && cpu->r[14] >= 0x08000000) {
                u32 failed_pc = cpu->r[14] - 4;
                printf("  Failed PC: 0x%08X (attempted to fetch instruction from here)\n", failed_pc);
                // Try to read what was there
                if (failed_pc >= 0x08000000 && failed_pc < 0x0A000000) {
                    u16 instr = mem_read16(mem, failed_pc);
                    printf("  Instruction bytes at failed PC: 0x%04X\n", instr);
                }
            }
            printf("      R0=%08X R1=%08X R2=%08X R3=%08X\n",
                   cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3]);
            bx_to_bios_count++;
            last_vector_pc = pc;
        }
        if (pc >= 0 && pc <= 0x10) {  // Exception vectors that should loop
            // Exception occurred (Reset, Undefined, SWI, Prefetch/Data Abort)
            // For prefetch abort (0x08), LR = failed instruction + 4
            // For data abort (0x0C), LR = failed instruction + 8
            // We should NOT retry the failed instruction - skip it!
            
            if (pc == 0x08) {
                // Prefetch abort - skip the problematic instruction
                // LR already points to next instruction (failed + 4)
                if (cpu->r[14] >= 0x08000000) {
                    // Debug: understand the prefetch abort addresses
                    static int pf_debug_count = 0;
                    if (pf_debug_count < 3) {
                        u32 lr = cpu->r[14];
                        // For Thumb: LR = (failed_instruction + 4) | 1
                        // So failed_instruction = (LR & ~1) - 4
                        u32 failed_addr = (lr & 0xFFFFFFFE) - 4;
                        printf("[BIOS] Prefetch abort #%d: LR=0x%08X, failed_addr=0x%08X\n", 
                               pf_debug_count, lr, failed_addr);
                        printf("      Current PC=0x%08X, Thumb=%d, CPSR=0x%08X\n",
                               cpu->r[15], cpu->thumb_mode, cpu->cpsr);
                        printf("      R0=%08X R1=%08X R2=%08X R3=%08X\n",
                               cpu->r[0], cpu->r[1], cpu->r[2], cpu->r[3]);
                        printf("      LR bit0=%d, failed_addr bit0=%d\n",
                               lr & 1, failed_addr & 1);
                        
                        u32 problematic_pc = failed_addr;
                        if (failed_addr & 1) {
                            printf("      !!! PROBLEM: Tried to fetch instruction from ODD address 0x%08X !!!\n", problematic_pc);
                            printf("      This means R15 was set to 0x%08X (odd!), which is invalid\n", problematic_pc + 4);
                        } else {
                            printf("      Failed to fetch instruction from EVEN address 0x%08X\n", problematic_pc);
                            printf("      This might be an unmapped/invalid memory region\n");
                        }
                        
                        pf_debug_count++;
                    }
                    
                    // Extract Thumb mode from LR bit 0
                    cpu->thumb_mode = cpu->r[14] & 1;
                    // Set PC with pipeline offset: R15 = target + (thumb ? 4 : 8)
                    u32 target = cpu->r[14] & 0xFFFFFFFE;
                    cpu->r[15] = target + (cpu->thumb_mode ? 4 : 8);
                    
                    // Verbose prefetch logging disabled
                    // Enable for debugging if needed
                    return 3;
                }
            }
            
            // For other exceptions, try to return to LR
            if (cpu->r[14] >= 0x08000000) {
                // Valid return address in ROM - return there
                cpu->thumb_mode = cpu->r[14] & 1;
                u32 target = cpu->r[14] & 0xFFFFFFFE;
                cpu->r[15] = target + (cpu->thumb_mode ? 4 : 8);
                return 3;
            } else {
                // No valid LR - jump to ROM entry
                cpu->r[15] = 0x08000000 + 8;  // ARM mode, so PC = target + 8
                cpu->thumb_mode = false;
                return 3;
            }
        } else {
            // IRQ/FIQ vectors at 0x18/0x1C - let them execute normally
            // Fall through to normal ARM execution
        }
    }
    
    if (cpu->thumb_mode) {
        u16 opcode = mem_read16(mem, pc - 4);  // Fetch from actual instruction address
        cpu->r[15] += 2;  // Increment PC before execution (pipeline)
        return execute_thumb(cpu, mem, opcode);
    } else {
        u32 opcode = mem_read32(mem, pc - 8);  // Fetch from actual instruction address
        cpu->r[15] += 4;  // Increment PC before execution (pipeline)
        return execute_arm(cpu, mem, opcode);
    }
}

void cpu_handle_interrupt(ARM7TDMI *cpu, Memory *mem) {
    // Check if IRQ is disabled in CPSR
    if (cpu->cpsr & FLAG_I) return;
    
    // Save current CPSR to SPSR_irq
    cpu->spsr = cpu->cpsr;
    
    // Switch to IRQ mode and disable IRQ
    cpu->cpsr = (cpu->cpsr & 0xFFFFFFE0) | 0x12; // IRQ mode
    cpu->cpsr |= FLAG_I; // Disable IRQ
    
    // Save return address in LR_irq (current PC + 4)
    cpu->r[14] = cpu->r[15] + 4;
    
    // Set PC to IRQ vector (0x00000018)
    cpu->r[15] = 0x00000018;
    cpu->thumb_mode = false;
    
    // Clear halted state
    cpu->halted = false;
}

void cpu_execute_frame(ARM7TDMI *cpu, Memory *mem, InterruptState *interrupts) {
    // GBA runs at ~16.78 MHz
    // At 60 FPS, that's ~280,000 cycles per frame
    const u32 CYCLES_PER_FRAME = 280000;
    
    u32 cycles_executed = 0;
    while (cycles_executed < CYCLES_PER_FRAME) {
        // Check for interrupts before each instruction
        if (interrupts && interrupt_check(interrupts)) {
            cpu_handle_interrupt(cpu, mem);
        }
        
        if (cpu->halted) {
            // If halted, just burn cycles until interrupt
            cycles_executed += 1;
            cpu->cycles += 1;
            continue;
        }
        
        u32 cycles = cpu_step(cpu, mem);
        cycles_executed += cycles;
        cpu->cycles += cycles;
    }
}
