#include <stdbool.h>
#include <stdlib.h> 
#include <stdio.h>
#include "common.h"
#include "cpu.h"
#include "cartridge.h"
#include "opcodes.h"
#include "disassembler.h"

/* GLOBAL STATE POINTERS */

static CPU *cpu;
static Register *R;
static uint8_t *memory;

/* STATIC (PRIVATE) HELPERS FUNCTIONS */

static void set_flag(bool is_set, Flag flag_mask)
{
    if (is_set) 
    {
        R->F |= flag_mask;  
    }
    else 
    { // Clear flag
        R->F &= ~flag_mask;
    }
}

static bool is_flag_set(Flag flag)
{
    return ((R->F & flag) == flag);
}

static uint8_t fetch()
{ // Fetch next part of ROM.
    pulse_clock();
    uint8_t memory = read_rom_memory(R->PC++);
    cpu->ins_length += 1;
    //if (cpu->testing) printf("\nFetched %02X from memory address %02X.\n", memory, R->PC);
    return memory;
}

static uint16_t getDR(DualRegister dr)
{
    switch(dr)
    {
        case AF_REG: return (R->A << BYTE) | R->F;
        case BC_REG: return (R->B << BYTE) | R->C;
        case DE_REG: return (R->D << BYTE) | R->E;
        case HL_REG: return (R->D << BYTE) | R->E;
    }
}

static void setDR(DualRegister dr, uint16_t source)
{
    switch(dr)
    {
        case AF_REG:
            R->A = ((uint8_t) (source >> BYTE));
            R->F = ((uint8_t) source);
            break;
        case BC_REG:
            R->B = ((uint8_t) (source >> BYTE));
            R->C = ((uint8_t) source);
            break;
        case DE_REG:
            R->D = ((uint8_t) (source >> BYTE));
            R->E = ((uint8_t) source);
            break;
        case HL_REG:
            R->H = ((uint8_t) (source >> BYTE));
            R->L = ((uint8_t) source);
            break;
    }
}

static void set_stack_pointer(uint16_t address)
{
    if (address < HIGH_RAM_ADDRESS_START) address = HIGH_RAM_ADDRESS_END;
    if (address > HIGH_RAM_ADDRESS_END) address = HIGH_RAM_ADDRESS_END;
    R->SP = address;
}

/* CPU OPCODE IMPLEMENTATION  */
static void no_op()
{ // 0x00 | -C
    /* Implicit pulse_clock() from fetch() */
}
static void halt() 
{ //0x76
    cpu->cpu_running = false;
    printf("\nPAUSED\n\n");
}
static void stop()
{ // 0x10 (- - - -)
    fetch();
}

static void reg_ld_16_nn(DualRegister dr)
{ // 0x01 - 0x31
    uint8_t upper_byte = fetch();
    uint8_t lower_byte = fetch();
    switch(dr)
    {
        case BC_REG: 
            R->B = upper_byte;
            R->C = lower_byte;
            break;
        case DE_REG: 
            R->D = upper_byte;
            R->E = lower_byte;
            break;
        case HL_REG:
            R->H = upper_byte;
            R->L = lower_byte;
            break;
        case SP_REG:
            set_stack_pointer((upper_byte << BYTE) | lower_byte);
            break;
    }
}
// Implemented Instructions. (----)
static void ld_bc_nn()
{ // 0x01 | -C
    reg_ld_16_nn(BC_REG); 
}
static void ld_de_nn()
{ // 0x11 (- - - -)
    reg_ld_16_nn(DE_REG); 
}
static void ld_hl_nn()
{ // 0x21
    reg_ld_16_nn(HL_REG);
}
static void ld_sp_nn()
{ // 0x31
    reg_ld_16_nn(SP_REG);
}

static void reg_inc_16(DualRegister dr)
{ // 0x03 - 0x33
    switch(dr)
    {
        case BC_REG: 
            R->C += 1;
            if (!R->C) R->B += 1; 
            break;
        case DE_REG: 
            R->D += 1;
            if (!R->D) R->E += 1; 
            break;
        case HL_REG:
            R->H += 1;
            if (!R->H) R->L += 1; 
            break;
        case SP_REG:
            set_stack_pointer(R->SP + 1);
            break;
    }
    pulse_clock();
}
// Implemented Instructions. (----)
static void inc_bc()
{ //0x03
    reg_inc_16(BC_REG); 
}
static void inc_de()
{ // 0x13 
    reg_inc_16(DE_REG);
}
static void inc_hl()
{ // 0x23
    reg_inc_16(HL_REG);  
}
static void inc_sp()
{ 
    reg_inc_16(SP_REG);
}

static void reg_dec_16(DualRegister dr)
{ // 0x0B - 0x3B
    switch(dr)
    {
        case BC_REG:
            R->C -= 1;
            if (R->C == BYTE_UNDERFLOW) R->B -= 1; 
            break;
        case DE_REG: 
            R->E -= 1;
            if (R->E == BYTE_UNDERFLOW) R->D -= 1;
            break;
        case HL_REG:
            R->L -= 1;
            if (R->L == BYTE_UNDERFLOW) R->H -= 1;
            break;
        case SP_REG:
            set_stack_pointer(R->SP - 1);
            break;
    }
    pulse_clock();
}
// Implemented Instructions. (----)
static void dec_bc()
{ // 0x0B | -C
    reg_dec_16(BC_REG); 
}
static void dec_de()
{ // 0x1B
    reg_dec_16(DE_REG);
}
static void dec_hl()
{ // 0x2B
    reg_dec_16(HL_REG); 
}
static void dec_sp()
{ 
    reg_dec_16(SP_REG); 
}

static uint8_t *read_memory(uint16_t address)
{
    pulse_clock();
    return &(memory[address]);
}

static void write_memory(uint16_t address, uint8_t value)
{ // THIS WRITES INTO COMPUTER MEMORY
    if (address < BANK_N_ADDRESS)
    {
        write_rom_memory(address, value);
    } 
    else 
    {
        memory[address] =  value;
    }
    pulse_clock();
}
// Implemented Instructions. (----)
static void ld_bc_a()
{ // 0x02 | -C
    write_memory(getDR(BC_REG), R->A);
}
static void ld_de_a()
{ // 
    write_memory(getDR(DE_REG), R->A); 
}
static void ld_hl_b()
{ // 0x70
    write_memory(getDR(HL_REG), R->B);
    pulse_clock();
}
static void ld_hl_c()
{ // 0x71
    write_memory(getDR(HL_REG), R->C);
    pulse_clock();
}
static void ld_hl_d()
{ // 0x72
    write_memory(getDR(HL_REG), R->D);
    pulse_clock();
}
static void ld_hl_e()
{ // 0x63
    write_memory(getDR(HL_REG), R->E);
    pulse_clock();    
}
static void ld_hl_h()
{ // 0x64
    write_memory(getDR(HL_REG), R->H);
    pulse_clock();
}
static void ld_hl_l()
{ //0x65
    write_memory(getDR(HL_REG), R->L);
    pulse_clock();
}
static void ld_hl_a()
{ //0x77
    write_memory(getDR(HL_REG), R->A);
    pulse_clock();   
}
static void ld_nn_sp()
{ // 0x08 | -C
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    write_memory((upper << BYTE | lower), R->SP);
}

static uint8_t pop_stack()
{
    uint8_t result = *read_memory(R->SP);
    set_stack_pointer(R->SP + 1);
    return result;
}
// Implemented Instructions. (- - - -)
static void pop_bc()
{ // 0xC1
    uint8_t upper = pop_stack();
    uint8_t lower = pop_stack();
    uint16_t result = (upper << BYTE) | lower;
    setDR(AF_REG, result);  
}
static void pop_de()
{ // 0xD1
    uint8_t upper = pop_stack();
    uint8_t lower = pop_stack();
    uint16_t result = (upper << BYTE) | lower;
    setDR(AF_REG, result);
}
static void pop_hl()
{  //0xE1
    uint8_t upper = pop_stack();
    uint8_t lower = pop_stack();
    uint16_t result = (upper << BYTE) | lower;
    setDR(AF_REG, result);
}
static void pop_af()
{ // 0xF1
    uint8_t upper = pop_stack();
    uint8_t lower = pop_stack();
    uint16_t result = (upper << BYTE) | lower;
    setDR(AF_REG, result);
}

static void push_stack(uint8_t value)
{
    write_memory(R->SP, value);
    set_stack_pointer(R->SP - 1);   
}
// Implemented Instructions. (- - - -)
static void push_bc()
{ // 0xC5
    push_stack(R->C);
    push_stack(R->B);
    pulse_clock();
}
static void push_de()
{ // 0xD5
    push_stack(R->E);
    push_stack(R->D);
    pulse_clock();
}
static void push_hl()
{ // 0xE5
    push_stack(R->L);
    push_stack(R->H);
    pulse_clock();
}
static void push_af()
{ // 0xF5
   push_stack(R->F);
   push_stack(R->A);
   pulse_clock();
}

static void reg_ld_8(uint8_t *r, uint8_t value)
{ // ~X0-X4 for 4-X-7 
    *r = value; 
}
// Implemented Instructions. (----)
static void ld_a_de()
{ // 0x1A
    R->A = *(read_memory(getDR(DE_REG)));
}
static void ld_hli_a()
{ // 0x22
    uint16_t hl = getDR(hl);
    write_memory(hl, R->A);
    setDR(HL_REG, hl + 1);
}
static void ld_a_hli()
{// 0x2A
    uint16_t hl = getDR(HL_REG);
    R->A = *(read_memory(hl));
    setDR(HL_REG, hl + 1);
}
static void ld_hld_a()
{ // 0x32 
    uint16_t hl = getDR(HL_REG);
    write_memory(hl, R->A);
    setDR(HL_REG, hl - 1);
}
static void ld_a_hld()
{
    uint16_t hl = getDR(HL_REG);
    R->A = *(read_memory(hl));
    setDR(HL_REG, hl - 1);
}
static void ld_hl_n()
{ // 0x36
    uint8_t n = fetch();
    write_memory(getDR(HL_REG), n);
}
// 0x4X
static void ld_b_c()
{ //0x41
    reg_ld_8(&(R->B), R->C);
}
static void ld_b_d()
{ // 0x42
    reg_ld_8(&(R->B), R->D);
}
static void ld_b_e()
{ // 0x43
    reg_ld_8(&(R->B), R->E);
}
static void ld_b_h()
{ // 0x44
    reg_ld_8(&(R->B), R->H);
}
static void ld_b_l()
{ // 0x45
    reg_ld_8(&(R->B), R->L);
}
static void ld_b_hl()
{ // 0x46
    reg_ld_8(&(R->B), *read_memory(getDR(HL_REG)));
    pulse_clock();
}
static void ld_b_a()
{ // 0x47
    reg_ld_8(&(R->B), R->A);
}
static void ld_c_b()
{ // 0x48
    reg_ld_8(&(R->C), R->B);
}
static void ld_c_d()
{ // 0x4A
    reg_ld_8(&(R->C), R->D);
}
static void ld_c_e()
{ // 0x4B
    reg_ld_8(&(R->C), R->E);
}
static void ld_c_h()
{ // 0x4C
    reg_ld_8(&(R->C), R->H);
}
static void ld_c_l()
{ // 0x4D
    reg_ld_8(&(R->C), R->L);
}
static void ld_c_hl()
{ // 0x4E
    reg_ld_8(&(R->C), *read_memory(getDR(HL_REG)));
    pulse_clock();
}
static void ld_c_a()
{ // 0x4F
    reg_ld_8(&(R->C), R->A);
}
// 0x5X
static void ld_d_b()
{ // 0x50
    reg_ld_8(&(R->D), R->B);
}
static void ld_d_c()
{ // 0x51
    reg_ld_8(&(R->D), R->C);
}
static void ld_d_e()
{ // 0x53
    reg_ld_8(&(R->D), R->D);
}
static void ld_d_h()
{ // 0x54
    reg_ld_8(&(R->D), R->H);
}
static void ld_d_l()
{ // 0x55
    reg_ld_8(&(R->D), R->L);
}
static void ld_d_hl()
{ // 0x56
    reg_ld_8(&(R->D), *read_memory(getDR(HL_REG)));
    pulse_clock();
}
static void ld_d_a()
{ // 0x57
    reg_ld_8(&(R->D), R->A);
}
static void ld_e_b()
{ // 0x58
    reg_ld_8(&(R->E), R->B);
}
static void ld_e_c()
{ // 0x59
    reg_ld_8(&(R->E), R->C);
}
static void ld_e_d()
{ // 0x5A
    reg_ld_8(&(R->E), R->D);
}
static void ld_e_h()
{ // 0x5C
    reg_ld_8(&(R->E), R->H);
}
static void ld_e_l()
{ // 0x5D
    reg_ld_8(&(R->E), R->L);
}
static void ld_e_hl()
{ // 0x5E
    reg_ld_8(&(R->E), *read_memory(getDR(HL_REG)));
    pulse_clock();
}
static void ld_e_a()
{ // 0x5F
    reg_ld_8(&(R->E), R->A);
}
// 0x6X
static void ld_h_b()
{ // 0x6A
    reg_ld_8(&(R->H), R->B);
}
static void ld_h_c()
{ //0x61
    reg_ld_8(&(R->H), R->C);
}
static void ld_h_d()
{ // 0x62
    reg_ld_8(&(R->H), R->D);
}
static void ld_h_e()
{ // 0x63
    reg_ld_8(&(R->H), R->E);
}
static void ld_h_l()
{ // 0x65
    reg_ld_8(&(R->H), R->L);
}
static void ld_h_hl()
{ // 0x66
    reg_ld_8(&(R->H), *read_memory(getDR(HL_REG)));
    pulse_clock();
}
static void ld_h_a()
{ // 0x67
    reg_ld_8(&(R->H), R->A);
}
static void ld_l_b()
{ // 0x68
    reg_ld_8(&(R->L), R->B);
}
static void ld_l_c()
{ // 0x69
    reg_ld_8(&(R->L), R->C);
}
static void ld_l_d()
{ //0x6A
    reg_ld_8(&(R->L), R->D);
}
static void ld_l_e()
{ //0x6B
    reg_ld_8(&(R->L), R->E);
}
static void ld_l_h()
{ //0x6C
    reg_ld_8(&(R->L), R->H);
}
static void ld_l_hl()
{ // 0x6E
    reg_ld_8(&(R->L), *read_memory(getDR(HL_REG)));
    pulse_clock();
}
static void ld_l_a()
{ //0x6F
    reg_ld_8(&(R->L), R->A);
}
// 0x7X
static void ld_a_b()
{ // 0x78
    reg_ld_8(&(R->A), R->B);
}
static void ld_a_c()
{ // 0x79
    reg_ld_8(&(R->A), R->C);
}
static void ld_a_d()
{ // 0x7A
    reg_ld_8(&(R->A), R->D);
}
static void ld_a_e()
{ // 0x7B
    reg_ld_8(&(R->A), R->B);
}
static void ld_a_h()
{ // 0x7C
    reg_ld_8(&(R->A), R->H);
}
static void ld_a_l()
{ // 0x7D
    reg_ld_8(&(R->A), R->L);
}
static void ld_a_hl()
{ // 0x7E
    reg_ld_8(&(R->A), *read_memory(getDR(HL_REG)));
    pulse_clock();
}
static void ld_a_bc()
{ // 0x0A | -C
    reg_ld_8(&(R->A), *read_memory(getDR(BC_REG)));
}

static void reg_ld_8_n(uint8_t *r)
{ // 0x06 - 0x36 AND 0x0A - 0x3A
     *r = fetch();
}
// Implemented Functions. (- - - -)
static void ld_b_n()
{ // 0x06 | -C
    reg_ld_8_n(&(R->B));
}
static void ld_d_n()
{ // 0x16 | -C
    reg_ld_8_n(&(R->D));
}
static void ld_h_n()
{ // 0x26 | -C
    reg_ld_8_n(&(R->H));
}
static void ld_c_n()
{ // 0x0E | -C
    reg_ld_8_n(&(R->C));
}
static void ld_e_n()
{ // 0x1E | -C
    reg_ld_8_n(&(R->E));
}
static void ld_l_n()
{ // 0x2E | -C
    reg_ld_8_n(&(R->L));
}
static void ld_a_n()
{ // 0x3E | -C
    reg_ld_8_n(&(R->A));
}

static void reg_inc_8(uint8_t *r)
{ // 0x04 - 0x34 AND 0x0C - 0x3C
    uint8_t result = *r + 1;
    bool is_zero = !((bool) result);
    bool hc_exists = (*r & LOWER_4_MASK) == LOWER_4_MASK;
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
}
// Implemented Functions. (Z 0 H -)
static void inc_b()
{ // 0x04 | -C
    reg_inc_8(&(R->B));
}
static void inc_d()
{ // 0x14 | -C
    reg_inc_8(&(R->D));
}
static void inc_h()
{ // 0x24 | -C
    reg_inc_8(&(R->H));
}
static void inc_hl_mem()
{ // 0x34 | -C
    uint8_t *value = read_memory(getDR(HL_REG));
    reg_inc_8(value); 
}
static void inc_c()
{ // 0x0C | -C
    reg_inc_8(&(R->C));
}
static void inc_e()
{ // 0x1C | -C
    reg_inc_8(&(R->E));
}
static void inc_l()
{ // 0x2C | -C
    reg_inc_8(&(R->L));
}
static void inc_a()
{ // 0x3C | -C
    reg_inc_8(&(R->A));
}

static void reg_dec_8(uint8_t *r)
{ // 0x05 - 0x35 AND 0x0B - 0x3B
    uint8_t result = *r - 1;
    bool is_zero = !((bool) result);
    bool hc_exists = (*r & LOWER_4_MASK) == IS_ZERO;
    set_flag(is_zero, ZERO_FLAG);
    set_flag(true, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
}
// Implemented Functions. (Z 1 H -)
static void dec_b()
{ // 0x05 | -C
    reg_dec_8(&(R->B));
}
static void dec_d()
{ // 0x15 | -C
    reg_dec_8(&(R->D));
}
static void dec_h()
{ // 0x25 | -C
    reg_dec_8(&(R->H));
}
static void dec_hl_mem()
{ // 0x35 | -C
    uint8_t *value = read_memory(getDR(HL_REG));
    reg_dec_8(value);
}
static void dec_c()
{ // 0x0D | -C
    reg_dec_8(&(R->C));
}
static void dec_e()
{ // 0x1D | -C
    reg_dec_8(&(R->E));
}
static void dec_l()
{ // 0x2D | -C
    reg_dec_8(&(R->L));
}
static void dec_a()
{ // 0x3D | -C
    reg_dec_8(&(R->A));
}

static void rla()
{ // 0x17 (- - - C)
    bool c_exists = R->A & BIT_7_MASK == BIT_7_MASK;
    set_flag(c_exists, CARRY_FLAG);
    R->A = (R->A << 1);
}
static void rlca()
{ // 0x07 (- - - C) | -C
    bool c_exists = R->A & BIT_7_MASK;
    set_flag(c_exists, CARRY_FLAG);
    R->A = ((R->A << 7) | (R->A >> 1));
}
static void rrca()
{ // 0x0F (- - - C) | -C
    bool c_exists = R->A & BIT_0_MASK;
    set_flag(c_exists, CARRY_FLAG);
    R->A = ((R->A << 7) | (R->A >> 1));
}
static void rra()
{ // 0x1F (- - - C)
    bool c_exists = (R->A & BIT_0_MASK) == BIT_0_MASK;
    set_flag(c_exists, CARRY_FLAG);
    R->A = R->A >> 1;
}

static uint16_t reg_add_16(uint16_t dest, uint16_t source)
{
    bool hc_exists =
    (dest & LOWER_12_MASK) + 
    (source & LOWER_12_MASK) > LOWER_12_MASK;
    bool c_exists = (dest + source) > MAX_INT_16;
    set_flag(false, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    pulse_clock();
    return (uint16_t) (dest + source);
}
// Implemented Functions (- 0 H C)
static void add_hl_bc()
{ // 0x09 | -C
    uint16_t result = reg_add_16(getDR(HL_REG), getDR(BC_REG));
    setDR(HL_REG, result);
}
static void add_hl_de()
{ // 0x19 | -C
    uint16_t result = reg_add_16(getDR(HL_REG), getDR(DE_REG));
    setDR(HL_REG, result);
}
static void add_hl_hl()
{ // 0x29 | -C
    uint16_t result = reg_add_16(getDR(HL_REG), getDR(HL_REG));
    setDR(HL_REG, result);
}
static void add_hl_sp()
{ // 0x39 | -C
    uint16_t result = reg_add_16(getDR(HL_REG), R->SP);
    setDR(HL_REG, result);
}

static uint8_t reg_add_8(uint8_t dest, uint8_t source, bool carry)
{ // 0x8X
    uint16_t result = dest + source + carry;
    bool is_zero = !((bool) result);
    bool hc_exists = 
    (dest & LOWER_4_MASK) + 
    (source & LOWER_4_MASK + carry) > LOWER_4_MASK;
    bool c_exists = result > MAX_INT_8;
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
    return (uint8_t) result;
}
// Implemented Functions. (Z 0 H C)
static void add_a_b()
{ // 0x80
    uint8_t result = reg_add_8(R->A, R->B, false);
    R->A = result;
}
static void add_a_c()
{ // 0x81
    uint8_t result = reg_add_8(R->A, R->C, false);
    R->A = result;
}
static void add_a_d()
{ // 0x82
    uint8_t result = reg_add_8(R->A, R->D, false);
    R->A = result;
}
static void add_a_e()
{ // 0x83
    uint8_t result = reg_add_8(R->A, R->E, false);
    R->A = result;
}
static void add_a_h()
{ // 0x84
    uint8_t result = reg_add_8(R->A, R->H, false);
    R->A = result;
}
static void add_a_l()
{ // 0x85
    uint8_t result = reg_add_8(R->A, R->L, false);
    R->A = result;
}
static void add_a_hl()
{ // 0x86
    uint8_t result = reg_add_8(R->A, *read_memory(getDR(HL_REG)), false);
    R->A = result;
}
static void add_a_a()
{ 
    uint8_t result = reg_add_8(R->A, R->A, false);
    R->A = result;
}
static void adc_a_b()
{  // 0x88
    uint8_t result = reg_add_8(R->A, R->B, true);
    R->A = result;
}
static void adc_a_c()
{ // 0x89
    uint8_t result = reg_add_8(R->A, R->C, true);
    R->A = result;
}
static void adc_a_d()
{ // 0x8A
    uint8_t result = reg_add_8(R->A, R->D, true);
    R->A = result;
}
static void adc_a_e()
{ // 0x8B
    uint8_t result = reg_add_8(R->A, R->E, true);
    R->A = result;
}
static void adc_a_h()
{ // 0x8C
    uint8_t result = reg_add_8(R->A, R->H, true);
    R->A = result;
}
static void adc_a_l()
{ // 0x8D
    uint8_t result = reg_add_8(R->A, R->L, true);
    R->A = result;
}
static void adc_a_hl()
{ // 0x8E
    uint8_t result = reg_add_8(R->A, getDR(HL_REG), true);
    R->A = result;
}
static void adc_a_a()
{ // 0x8F
    uint8_t result = reg_add_8(R->A, R->A, true);
    R->A = result;
}
static void add_a_n()
{ // 0xC6
   uint8_t result = reg_add_8(R->A, fetch(), false);
   R->A = result;
}
static void adc_a_n()
{ // 0xCE
    uint8_t result = reg_add_8(R->A, fetch(), true);
    R->A = result;
}

static uint8_t reg_sub_8(uint8_t dest, uint8_t source, bool carry)
{ // 0x9X
    uint8_t result = dest - source - carry;
    set_flag(true, SUBTRACT_FLAG);
    bool is_zero = !((bool) result);
    set_flag(is_zero, ZERO_FLAG);
    bool hc_exists = (dest & LOWER_4_MASK) < ((source & LOWER_4_MASK) + carry);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    bool c_exists = dest < (source + carry);
    set_flag(c_exists, CARRY_FLAG);
    return result;
}
// Implemented Functions. (Z 1 H C)
static void sub_a_b()
{ // 0x90
    uint8_t diff = reg_sub_8(R->A, R->B, false);
    R->A = diff;
}
static void sub_a_c()
{ // 0x91
     uint8_t diff = reg_sub_8(R->A, R->C, false);
    R->A = diff;
}
static void sub_a_d()
{ // 0x92
    uint8_t diff = reg_sub_8(R->A, R->D, false);
    R->A = diff;
}
static void sub_a_e()
{ // 0x93
    uint8_t diff = reg_sub_8(R->A, R->E, false);
    R->A = diff;
}
static void sub_a_h()
{ // 0x94
    uint8_t diff = reg_sub_8(R->A, R->H, false);
    R->A = diff;
}
static void sub_a_l()
{ // 0x95
    uint8_t diff = reg_sub_8(R->A, R->L, false);
    R->A = diff;
}
static void sub_a_hl()
{ // 0x96
    uint8_t diff = reg_sub_8(R->A, *read_memory(getDR(HL_REG)), false);
    R->A = diff;
}
static void sub_a_a()
{ // 0x97
    uint8_t diff = reg_sub_8(R->A, R->A, false);
    R->A = diff;
}
static void sbc_a_b()
{ // 0x98
    uint8_t diff = reg_sub_8(R->A, R->B, true);
    R->A = diff;
}
static void sbc_a_c()
{ // 0x99
    uint8_t diff = reg_sub_8(R->A, R->C, true);
    R->A = diff;
}
static void sbc_a_d()
{ // 0x9A
    uint8_t diff = reg_sub_8(R->A, R->D, true);
    R->A = diff;
}
static void sbc_a_e()
{ // 0x9B
    uint8_t diff = reg_sub_8(R->A, R->E, true);
    R->A = diff;
}
static void sbc_a_h()
{ // 0x9C
    uint8_t diff = reg_sub_8(R->A, R->H, true);
    R->A = diff;
}
static void sbc_a_l()
{ // 0x9D
    uint8_t diff = reg_sub_8(R->A, R->L, true);
    R->A = diff;
}
static void sbc_a_hl()
{ // 0x9E
    uint8_t diff = reg_sub_8(R->A, *read_memory(getDR(HL_REG)), true);
    R->A = diff;
}
static void sbc_a_a()
{ // 0x9F
    uint8_t diff = reg_sub_8(R->A, R->A, true);
    R->A = diff;
}
static void sub_a_n()
{ // 0xD6
    uint8_t result = reg_sub_8(R->A, fetch(), false);
    R->A = result;
}
static void sbc_a_n()
{ // 0xDE
    uint8_t result = reg_sub_8(R->A, fetch(), true);
    R->A = result;   
}

static uint8_t reg_and_8(uint8_t dest, uint8_t source)
{ // 0xA0 - 0xA7
    uint8_t result = dest & source;
    bool is_zero = !((bool) result);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(true, HALF_CARRY_FLAG);
    set_flag(false, CARRY_FLAG);
    return result;
}
// Implemented Functions. (Z 0 1 0)
static void and_a_b()
{ // 0xA0
    uint8_t result = reg_and_8(R->A, R->B);
    R->A = result; 
}
static void and_a_c()
{ // 0xA1
    uint8_t result = reg_and_8(R->A, R->C);
    R->A = result; 
}
static void and_a_d()
{ // 0xA2
    uint8_t result = reg_and_8(R->A, R->D);
    R->A = result; 
}
static void and_a_e()
{ // 0xA3
    uint8_t result = reg_and_8(R->A, R->E);
    R->A = result; 
}
static void and_a_h()
{ // 0xA4
    uint8_t result = reg_and_8(R->A, R->H);
    R->A = result; 
}
static void and_a_l()
{ // 0xA5
    uint8_t result = reg_and_8(R->A, R->L);
    R->A = result; 
}
static void and_a_hl()
{ // 0xA6
    uint8_t result = reg_and_8(R->A, *read_memory(getDR(HL_REG)));
    R->A = result; 
}
static void and_a_a()
{ // 0xA7
    uint8_t result = reg_and_8(R->A, R->A);
    R->A = result; 
}
static void and_a_n()
{ // 0xE6
    uint8_t result = reg_and_8(R->A, fetch());
    R->A = result;
}

static uint8_t reg_xor_8(uint8_t dest, uint8_t source)
{ // 0xA8 - 0xAF
    uint8_t result = dest ^ source;
    bool is_zero = !((bool) result);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(false, CARRY_FLAG);
}
// Implemented Functions. (Z 0 0 0)
static void xor_a_b()
{ // 0xA8
    uint8_t result = reg_xor_8(R->A, R->B);
    R->A = result;
}
static void xor_a_c()
{ // 0xA9
    uint8_t result = reg_xor_8(R->A, R->C);
    R->A = result;
}
static void xor_a_d()
{ // 0xAA
    uint8_t result = reg_xor_8(R->A, R->D);
    R->A = result;
}
static void xor_a_e()
{ // 0xAB
    uint8_t result = reg_xor_8(R->A, R->E);
    R->A = result;
}
static void xor_a_h()
{ // 0xAC
    uint8_t result = reg_xor_8(R->A, R->H);
    R->A = result;
}
static void xor_a_l()
{ // 0x7D
    uint8_t result = reg_xor_8(R->A, R->L);
    R->A = result;
}
static void xor_a_hl()
{ // 0xAE
    uint8_t result = reg_xor_8(R->A, *read_memory(getDR(HL_REG)));
    R->A = result;
}
static void xor_a_a()
{ // 0xAF
    uint8_t result = reg_xor_8(R->A, R->A);
    R->A = result;
}
static void xor_a_n()
{ // 0xEE
    uint8_t result = reg_xor_8(R->A, fetch());
    R->A = result;
}

static uint8_t reg_or_8(uint8_t dest, uint8_t source)
{ // 0xB0 - 0xB7
    uint8_t result = dest | source;
    bool is_zero = !((bool) result);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(false, CARRY_FLAG);
    return result;
}
// Implemented Functions. (Z 0 0 0)
static void or_a_b()
{ // 0xB0
    uint8_t result = reg_or_8(R->A, R->B);
    R->A = result;
}
static void or_a_c()
{ // 0xB1
    uint8_t result = reg_or_8(R->A, R->C);
    R->A = result;
}
static void or_a_d()
{ // 0xB2
    uint8_t result = reg_or_8(R->A, R->D);
    R->A = result;
}
static void or_a_e()
{ // 0xB3
    uint8_t result = reg_or_8(R->A, R->E);
    R->A = result;
}
static void or_a_h()
{ // 0xB4
    uint8_t result = reg_or_8(R->A, R->H);
    R->A = result;
}
static void or_a_l()
{ // 0xB5
    uint8_t result = reg_or_8(R->A, R->L);
    R->A = result;
}
static void or_a_hl()
{ // 0xB6
    uint8_t result = reg_or_8(R->A, *read_memory(getDR(HL_REG)));
    R->A = result;
}
static void or_a_a()
{ // 0xB7
    uint8_t result = reg_or_8(R->A, R->A);
    R->A = result;
}
static void or_a_n()
{ // 0xF6
    uint8_t result = reg_or_8(R->A, fetch());
    R->A = result;
}

static void reg_cp_8(uint8_t dest, uint8_t source)
{ // 0xB8 - 0xBF
    uint8_t result = dest - source;
    bool is_zero = !((bool) result);
    bool hc_exists = (dest & LOWER_4_MASK) < (source & LOWER_4_MASK);
    bool c_exists = dest < source;
    set_flag(is_zero, ZERO_FLAG);
    set_flag(true, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
}
// Implemented Functions. (Z 1 H C)
static void cp_a_b()
{ // 0xB8
    reg_cp_8(R->A, R->B);
}
static void cp_a_c()
{ // 0xB9
    reg_cp_8(R->A, R->C);
}
static void cp_a_d()
{ // 0xBA
    reg_cp_8(R->A, R->D);
}
static void cp_a_e()
{ // 0xBB
    reg_cp_8(R->A, R->E);
}
static void cp_a_h()
{ // 0xBC
    reg_cp_8(R->A, R->H);
}
static void cp_a_l()
{ // 0xBD
    reg_cp_8(R->A, R->L);
}
static void cp_a_hl()
{ // 0xBE
    reg_cp_8(R->A, *read_memory(getDR(HL_REG)));
}
static void cp_a_a()
{ // 0xBF
    reg_cp_8(R->A, R->A);
}
static void cp_a_n()
{ // 0xFE
    reg_cp_8(R->A, fetch());
}

static void ld_pc_stack()
{ // loads next two bytes from stack to PC.
    uint8_t upper = pop_stack();
    uint8_t lower = pop_stack();
    uint16_t result = (upper << BYTE) | lower;
    set_stack_pointer(result);
    pulse_clock(); 
}
// Implemented Instruction. (- - - -)
static void ret_nz()
{ // 0xC0
    if (!is_flag_set(ZERO_FLAG)) ld_pc_stack();
    pulse_clock();
}
static void ret_nc()
{ // 0xD0
    if (!is_flag_set(CARRY_FLAG)) ld_pc_stack();
    pulse_clock();
}
static void ret_c()
{ // 0xD8
    if (is_flag_set(CARRY_FLAG)) ld_pc_stack();
    pulse_clock();
}
static void ret_z()
{ // 0xC8
    if (is_flag_set(ZERO_FLAG)) ld_pc_stack();
    pulse_clock();
}
static void ret()
{ // 0xC9
    ld_pc_stack();
}
static void reti()
{ // 0xD9
    cpu->interrupt_enabled = true;
    ld_pc_stack();
}

static void ld_stack_pc()
{ // Loads PC onto stack. 0xC7-0xF7 & 0xF7-0xFF
    uint8_t upper = (R->PC >> BYTE) & LOWER_BYTE_MASK;
    uint8_t lower = R->PC & LOWER_BYTE_MASK;
    push_stack(lower);
    push_stack(upper);
    pulse_clock();
}
// Implemented functions. (- - - -)
static void rst_00()
{ // 0xC7
    ld_stack_pc();
    R->PC = 0x00;
}
static void rst_10()
{ // D7
    ld_stack_pc();
    R->PC = 0x10;
}
static void rst_20()
{
    ld_stack_pc();
    R->PC = 0x20;
}
static void rst_30()
{
    ld_stack_pc();
    R->PC = 0x30;
}
static void rst_08()
{ // 0xCF
    ld_stack_pc();
    R->PC = 0x08;
}
static void rst_18()
{ // 0xDF
    ld_stack_pc();
    R->PC = 0x18;
}
static void rst_28()
{ // 0xEF
    ld_stack_pc();
    R->PC = 0x28;
}
static void rst_38()
{ // 0xFF
    ld_stack_pc();
    R->PC = 0x38;
}
static void call_nz_nn()
{ // 0xC4
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    if(!is_flag_set(ZERO_FLAG))
    {
        ld_stack_pc();
        uint16_t result = (upper << BYTE) | lower;
        R->PC = result;
    }
}
static void call_nc_nn()
{ // 0xC=D4
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    if(!is_flag_set(CARRY_FLAG))
    {
        ld_stack_pc();
        uint16_t result = (upper << BYTE) | lower;
        R->PC = result;
    }
}
static void call_z_nn()
{ // 0xCC
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    if(is_flag_set(ZERO_FLAG))
    {
        ld_stack_pc();
        uint16_t result = (upper << BYTE) | lower;
        R->PC = result;
    }
}
static void call_c_nn()
{ // 0xDC
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    if(is_flag_set(CARRY_FLAG))
    {
        ld_stack_pc();
        uint16_t result = (upper << BYTE) | lower;
        R->PC = result;
    }
}
static void call_nn()
{ // 0xCD
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    ld_stack_pc();
    uint16_t result = (upper << BYTE) | lower;
    R->PC = result;
}

static void jr_n()
{ // 0x18 (- - - -)
    int8_t offset = (int8_t) fetch();
    R->PC += offset;
    pulse_clock();
}
static void jr_z_n()
{ // 0x28 | (- - - -)
    if(is_flag_set(ZERO_FLAG))
    {
        int8_t offset = (int8_t) fetch();
        R->PC += offset;
    }
    pulse_clock(); 
}
static void jr_c_n()
{ // 0x38 (- - - -)
    if(is_flag_set(CARRY_FLAG))
    {
        int8_t offset = (int8_t) fetch();
        R->PC += offset;
    }
    pulse_clock();
}
static void jr_nz_n()
{ // 0x20 (- - - -)
    if(!is_flag_set(ZERO_FLAG))
    {
        int8_t offset = (int8_t) fetch();
        R->PC += offset;
    }
    pulse_clock();
}
static void jr_nc_n()
{ // 0x30
    if(!is_flag_set(CARRY_FLAG))
    {
        int8_t offset = (int8_t) fetch();
        R->PC += offset;
    }
    pulse_clock();
}
static void jp_nz_nn()
{ // 0xC2
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    if(!is_flag_set(ZERO_FLAG))
    {
        uint16_t result = (upper << BYTE) | lower;
        R->PC = result;
        pulse_clock();
    }
}
static void jp_nc_nn()
{ // 0xD2
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    if(!is_flag_set(CARRY_FLAG))
    {
        uint16_t result = (upper << BYTE) | lower;
        R->PC = result;
        pulse_clock();
    }
}
static void jp_nn()
{ // C3
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    uint16_t result = (upper << BYTE) | lower;
    R->PC = result;
    pulse_clock();
}
static void jp_z_nn()
{ // 0xCA
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    if(is_flag_set(ZERO_FLAG))
    {
        uint16_t result = (upper << BYTE) | lower;
        R->PC = result;
        pulse_clock();
    }
}
static void jp_c_nn()
{
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    if(is_flag_set(CARRY_FLAG))
    {
        uint16_t result = (upper << BYTE) | lower;
        R->PC = result;
        pulse_clock();
    }
}
static void jp_hl()
{ // 0xE9
    R->PC = getDR(HL_REG);
}

static void daa()
{ //0x27 (Z - 0 C)
  // Changes a to corrected decimal value.
}
static void cpl()
{ // 0x2F (- 1 1 -)
    R->A = ~R->A;
    set_flag(true, SUBTRACT_FLAG);
    set_flag(true, HALF_CARRY_FLAG);
}
static void scf()
{ // 0x07 (- 0 0 1)
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(true, CARRY_FLAG);
}
static void ccf()
{ // 0x3F (- 0 0 C)
    set_flag(false, SUBTRACT_FLAG);
    set_flag(false, HALF_CARRY_FLAG);
    set_flag(!is_flag_set(CARRY_FLAG), CARRY_FLAG);
}

static void reg_ldh_8(uint8_t dest, uint8_t value)
{
    write_memory(IO_REGISTERS_START + dest, value);
}
// Implemented Functions. (- - - -)
static void ldh_n_a()
{ // 0xE0
    reg_ldh_8(*read_memory(fetch()), R->A);
}
static void ldh_a_n()
{ // 0xF0
    reg_ldh_8(R->A, *read_memory(fetch()));
}
static void ldh_c_a()
{ // 0xE1
    reg_ldh_8(*read_memory(R->C), R->A);
}
static void ldh_a_c()
{ // 0xF1
    reg_ldh_8(R->A, *read_memory(R->C));
}

static void ld_nn_a()
{ // 0xEA
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    uint16_t address = (upper << BYTE) | lower;
    write_memory(address, R->A);
}
static void ld_a_nn()
{ // 0xFA
    uint8_t upper = fetch();
    uint8_t lower = fetch();
    uint16_t address = (upper << BYTE) | lower;
    R->A = *read_memory(address); 
}
static void ld_hl_sp_n()
{ // 0xF8 (0 0 H C)
    int8_t n = (int8_t) fetch();
    uint16_t result = R->SP + n;
    bool hc_exists = ((R->SP & LOWER_BYTE_MASK) + n) > LOWER_BYTE_MASK;
    bool c_exits = (R->SP + n) > MAX_INT_16;
    set_flag(false, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    set_flag(c_exits, CARRY_FLAG);
    setDR(HL_REG, result);
    pulse_clock(); 
}
static void ld_sp_hl()
{
    set_stack_pointer(getDR(HL_REG));
}
static void add_sp_n()
{ // 0xE7 (0 0 H C)
    int8_t n = (int8_t) fetch();
    uint16_t sum = (uint16_t) (R->SP + n);
    bool hc_exists = ((R->SP & LOWER_BYTE_MASK) + n) > LOWER_BYTE_MASK;
    bool c_exists  = (R->SP + n) > MAX_INT_16;
    set_flag(false, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(hc_exists, HALF_CARRY_FLAG);
    set_flag(c_exists, CARRY_FLAG);
}

static void di()
{ // 0xF3
    cpu->interrupt_enabled = false;
}
static void ei()
{ // 0xFB Enables after next instruction.
    cpu->interrupt_enabled = IE_NEXT_INS;
}

static void print_registers()
{ // 0x43
    printf("\n-CPU REGISTERS-\n");
    printf("A -> %02X | ", R->A);
    printf("%02X <- F\n", R->F);
    printf("B -> %02X | ", R->B); 
    printf("%02X <- C\n", R->C);
    printf("D -> %02X | ", R->D); 
    printf("%02X <- E\n", R->E);
    printf("H -> %02X | ", R->H); 
    printf("%02X <- L\n", R->L);
    printf("PC -> %02X\n", R->PC);
    printf("SP -> %02X\n\n         -FLAGS-\n", R->SP);
    printf("(Z - %d)" , (ZERO_FLAG & R->F == ZERO_FLAG));
    printf(" (N - %d)", (SUBTRACT_FLAG & R->F) == SUBTRACT_FLAG);
    printf(" (H - %d)", (HALF_CARRY_FLAG & R->F) == HALF_CARRY_FLAG);
    printf(" (C - %d)\n\n", (CARRY_FLAG & R->F) == CARRY_FLAG);
}

typedef void (*OpcodeHandler)();
OpcodeHandler opcode_table[256] = 
{
    /*              ROW 1               */
    /* 0x00 */ [NOP]       = no_op,   
    /* 0x01 */ [LD_BC_NN]  = ld_bc_nn,
    /* 0x02 */ [LD_BC_A]   = ld_bc_a,
    /* 0x03 */ [INC_BC]    = inc_bc,
    /* 0x04 */ [INC_B]     = inc_b,
    /* 0x05 */ [DEC_B]     = dec_b,
    /* 0x06 */ [LD_B_N]    = ld_b_n,
    /* 0x07 */ [RLCA]      = rlca,
    /* 0x08 */ [LD_NN_SP]  = ld_nn_sp,
    /* 0x09 */ [ADD_HL_BC] = add_hl_bc,
    /* 0x0A */ [LD_A_BC]   = ld_a_bc,
    /* 0x0B */ [DEC_BC]    = dec_bc,
    /* 0x0C */ [INC_C]     = inc_c,
    /* 0x0D */ [DEC_C]     = dec_c,
    /* 0x0E */ [LD_C_N]    = ld_c_n,
    /* 0x0F */ [RRCA]      = rrca,
    /*              ROW 2                */
    /* 0x10 */ [STOP]      = stop,   
    /* 0x11 */ [LD_DE_NN]  = ld_de_nn,
    /* 0x12 */ [LD_DE_A]   = ld_de_a,
    /* 0x13 */ [INC_DE]    = inc_de,
    /* 0x14 */ [INC_D]     = inc_d,
    /* 0x15 */ [DEC_D]     = dec_d,
    /* 0x16 */ [LD_D_N]    = ld_d_n,
    /* 0x17 */ [RLA]       = rla,
    /* 0x18 */ [JR_N]      = jr_n,
    /* 0x19 */ [ADD_HL_DE] = add_hl_de,
    /* 0x1A */ [LD_A_DE]   = ld_a_de,
    /* 0x1B */ [DEC_DE]    = dec_de,
    /* 0x1C */ [INC_E]     = inc_e,
    /* 0x1D */ [DEC_E]     = dec_e,
    /* 0x1E */ [LD_E_N]    = ld_e_n,
    /* 0x1F */ [RRA]       = rra,
    /*              ROW 3                */
    /* 0x20 */ [JR_NZ_N]   = jr_nz_n,   
    /* 0x21 */ [LD_HL_NN]  = ld_hl_nn,
    /* 0x22 */ [LD_HLI_A]  = ld_hli_a,
    /* 0x23 */ [INC_HL]    = inc_hl,
    /* 0x24 */ [INC_H]     = inc_h,
    /* 0x25 */ [DEC_H]     = dec_h,
    /* 0x26 */ [LD_H_N]    = ld_h_n,
    /* 0x27 */ [DAA]       = daa,
    /* 0x28 */ [JR_Z_N]    = jr_z_n,
    /* 0x29 */ [ADD_HL_HL] = add_hl_hl,
    /* 0x2A */ [LD_A_HLI]  = ld_a_hli,
    /* 0x2B */ [DEC_HL]    = dec_hl,
    /* 0x2C */ [INC_L]     = inc_l,
    /* 0x2D */ [DEC_L]     = dec_l,
    /* 0x2E */ [LD_L_N]    = ld_l_n,
    /* 0x2F */ [CPL]       = cpl,
    /*              ROW 4                */
    /* 0x30 */ [JR_NC_N]   = jr_nc_n,   
    /* 0x31 */ [LD_SP_NN]  = ld_sp_nn,
    /* 0x32 */ [LD_HLD_A]  = ld_hld_a,
    /* 0x33 */ [INC_SP]    = inc_sp,
    /* 0x34 */ [INC_HL_MEM]= inc_hl_mem,
    /* 0x35 */ [DEC_HL_MEM]= dec_hl_mem,
    /* 0x36 */ [LD_HL_N]   = ld_hl_n,
    /* 0x37 */ [SCF]       = scf,
    /* 0x38 */ [JR_C_N]    = jr_c_n,
    /* 0x39 */ [ADD_HL_SP] = add_hl_sp,
    /* 0x3A */ [LD_A_HLD]  = ld_a_hld,
    /* 0x3B */ [DEC_SP]    = dec_sp,
    /* 0x3C */ [INC_A]     = inc_a,
    /* 0x3D */ [DEC_A]     = dec_a,
    /* 0x3E */ [LD_A_N]    = ld_a_n,
    /* 0x3F */ [CCF]       = ccf,
    /*              ROW 5                */
    /* 0x40 */ [LD_B_B]    = no_op,   
    /* 0x41 */ [LD_B_C]    = ld_b_c,
    /* 0x42 */ [LD_B_D]    = ld_b_d,
    /* 0x43 */ [LD_B_E]    = ld_b_e,
    /* 0x44 */ [LD_B_H]    = ld_b_h,
    /* 0x45 */ [LD_B_L]    = ld_b_l,
    /* 0x46 */ [LD_B_HL]   = ld_b_hl,
    /* 0x47 */ [LD_B_A]    = ld_b_a,
    /* 0x48 */ [LD_C_B]    = ld_c_b,
    /* 0x49 */ [LD_C_C]    = no_op,
    /* 0x4A */ [LD_C_D]    = ld_c_d,
    /* 0x4B */ [LD_C_E]    = ld_c_e,
    /* 0x4C */ [LD_C_H]    = ld_c_h,
    /* 0x4D */ [LD_C_L]    = ld_c_l,
    /* 0x4E */ [LD_L_HL]   = ld_l_hl,
    /* 0x4F */ [LD_C_A]    = ld_c_a,
    /*              ROW 6                */
    /* 0x50 */ [LD_D_B]    = ld_d_b,   
    /* 0x51 */ [LD_D_C]    = ld_d_c,
    /* 0x52 */ [LD_D_D]    = no_op,
    /* 0x53 */ [LD_D_E]    = ld_d_e,
    /* 0x54 */ [LD_D_H]    = ld_d_h,
    /* 0x55 */ [LD_D_L]    = ld_d_l,
    /* 0x56 */ [LD_D_HL]   = ld_d_hl,
    /* 0x57 */ [LD_D_A]    = ld_d_a,
    /* 0x58 */ [LD_E_B]    = ld_e_b,
    /* 0x59 */ [LD_E_C]    = ld_e_c,
    /* 0x5A */ [LD_E_D]    = ld_e_d,
    /* 0x5B */ [LD_E_E]    = no_op,
    /* 0x5C */ [LD_E_H]    = ld_e_h,
    /* 0x5D */ [LD_E_L]    = ld_e_l,
    /* 0x5E */ [LD_E_HL]   = ld_e_hl,
    /* 0x5F */ [LD_E_A]    = ld_e_a,
    /*              ROW 7                */
    /* 0x60 */ [LD_H_B]    = ld_h_b,   
    /* 0x61 */ [LD_H_C]    = ld_h_c,
    /* 0x62 */ [LD_H_D]    = ld_h_d,
    /* 0x63 */ [LD_H_E]    = ld_h_e,
    /* 0x64 */ [LD_H_H]    = no_op,
    /* 0x65 */ [LD_H_L]    = ld_h_l,
    /* 0x66 */ [LD_H_HL]   = ld_h_hl,
    /* 0x67 */ [LD_H_A]    = ld_h_a,
    /* 0x68 */ [LD_H_B]    = ld_h_b,
    /* 0x69 */ [LD_L_C]    = ld_l_c,
    /* 0x6A */ [LD_L_D]    = ld_l_d,
    /* 0x6B */ [LD_L_E]    = ld_l_e,
    /* 0x6C */ [LD_L_H]    = ld_l_h,
    /* 0x6D */ [LD_L_L]    = no_op,
    /* 0x6E */ [LD_L_HL]   = ld_l_hl,
    /* 0x6F */ [LD_L_A]    = ld_l_a,
    /*              ROW 8                */
    /* 0x70 */ [LD_HL_B]   = ld_hl_b,   
    /* 0x71 */ [LD_HL_C]   = ld_hl_c,
    /* 0x72 */ [LD_HL_D]   = ld_hl_d,
    /* 0x73 */ [LD_HL_E]   = ld_hl_e,
    /* 0x74 */ [LD_HL_H]   = ld_hl_h,
    /* 0x75 */ [LD_HL_L]   = ld_hl_l,
    /* 0x76 */ [HALT]      = halt,
    /* 0x77 */ [LD_HL_A]   = ld_hl_a,
    /* 0x78 */ [LD_A_B]    = ld_a_b,
    /* 0x79 */ [LD_A_C]    = ld_a_c,
    /* 0x7A */ [LD_A_D]    = ld_a_d,
    /* 0x7B */ [LD_A_E]    = ld_a_e,
    /* 0x7C */ [LD_A_H]    = ld_a_h,
    /* 0x7D */ [LD_A_L]    = ld_a_l,
    /* 0x7E */ [LD_A_HL]   = ld_a_hl,
    /* 0x7F */ [LD_A_A]    = no_op,
    /*              ROW 9                */
    /* 0x80 */ [ADD_A_B]   = add_a_b,   
    /* 0x81 */ [ADD_A_C]   = add_a_c,
    /* 0x82 */ [ADD_A_D]   = add_a_d,
    /* 0x83 */ [ADD_A_E]   = add_a_e,
    /* 0x84 */ [ADD_A_H]   = add_a_h,
    /* 0x85 */ [ADD_A_L]   = add_a_l,
    /* 0x86 */ [ADD_A_HL]  = add_a_hl,
    /* 0x87 */ [ADD_A_A]   = adc_a_a,
    /* 0x88 */ [ADC_A_B]   = adc_a_b,
    /* 0x89 */ [ADC_A_C]   = adc_a_c,
    /* 0x8A */ [ADC_A_D]   = adc_a_d,
    /* 0x8B */ [ADC_A_E]   = adc_a_e,
    /* 0x8C */ [ADC_A_H]   = adc_a_h,
    /* 0x8D */ [ADC_A_L]   = adc_a_l,
    /* 0x8E */ [ADC_A_HL]  = adc_a_hl,
    /* 0x8F */ [ADC_A_A]   = adc_a_a,
    /*              ROW 10                */
    /* 0x90 */ [SUB_A_B]   = sub_a_b,   
    /* 0x91 */ [SUB_A_C]   = sub_a_c,
    /* 0x92 */ [SUB_A_D]   = sub_a_d,
    /* 0x93 */ [SUB_A_E]   = sub_a_e,
    /* 0x94 */ [SUB_A_H]   = sub_a_h,
    /* 0x95 */ [SUB_A_L]   = sub_a_l,
    /* 0x96 */ [SUB_A_HL]  = sub_a_hl,
    /* 0x97 */ [SUB_A_A]   = sub_a_a,
    /* 0x98 */ [SBC_A_B]   = sbc_a_b,
    /* 0x99 */ [SBC_A_C]   = sbc_a_c,
    /* 0x9A */ [SBC_A_D]   = sbc_a_d,
    /* 0x9B */ [SBC_A_E]   = sbc_a_e,
    /* 0x9C */ [SBC_A_H]   = sbc_a_h,
    /* 0x9D */ [SBC_A_L]   = sbc_a_l,
    /* 0x9E */ [SBC_A_HL]  = sbc_a_hl,
    /* 0x9F */ [SBC_A_A]   = sbc_a_a,
    /*              ROW 11                */
    /* 0xA0 */ [AND_A_B]   = and_a_b,   
    /* 0xA1 */ [AND_A_C]   = and_a_c,
    /* 0xA2 */ [AND_A_D]   = and_a_d,
    /* 0xA3 */ [AND_A_E]   = and_a_e,
    /* 0xA4 */ [AND_A_H]   = and_a_h,
    /* 0xA5 */ [AND_A_L]   = and_a_l,
    /* 0xA6 */ [AND_A_HL]  = and_a_hl,
    /* 0xA7 */ [AND_A_A]   = and_a_a,
    /* 0xA8 */ [XOR_A_B]   = xor_a_b,
    /* 0xA9 */ [XOR_A_C]   = xor_a_c,
    /* 0xAA */ [XOR_A_D]   = xor_a_d,
    /* 0xAB */ [XOR_A_E]   = xor_a_e,
    /* 0xAC */ [XOR_A_H]   = xor_a_h,
    /* 0xAD */ [XOR_A_L]   = xor_a_l,
    /* 0xAE */ [XOR_A_HL]  = xor_a_hl,
    /* 0xAF */ [XOR_A_A]   = xor_a_a,
    /*              ROW 12                */
    /* 0xB0 */ [OR_A_B]    = or_a_b,   
    /* 0xB1 */ [OR_A_C]    = or_a_c,
    /* 0xB2 */ [OR_A_D]    = or_a_d,
    /* 0xB3 */ [OR_A_E]    = or_a_e,
    /* 0xB4 */ [OR_A_H]    = or_a_h,
    /* 0xB5 */ [OR_A_L]    = or_a_l,
    /* 0xB6 */ [OR_A_HL]   = or_a_hl,
    /* 0xB7 */ [OR_A_A]    = or_a_a,
    /* 0xB8 */ [CP_A_B]    = cp_a_b,
    /* 0xB9 */ [CP_A_C]    = cp_a_c,
    /* 0xBA */ [CP_A_D]    = cp_a_d,
    /* 0xBB */ [CP_A_E]    = cp_a_e,
    /* 0xBC */ [CP_A_H]    = cp_a_h,
    /* 0xBD */ [CP_A_L]    = cp_a_l,
    /* 0xBE */ [CP_A_HL]   = cp_a_hl,
    /* 0xBF */ [CP_A_A]    = cp_a_a,
    /*              ROW 13                */
    /* 0xC0 */ [RET_NZ]    = ret_nz,   
    /* 0xC1 */ [POP_BC]    = pop_bc,
    /* 0xC2 */ [JP_NZ_NN]  = jp_nz_nn,
    /* 0xC3 */ [JP_NN]     = jp_nn,
    /* 0xC4 */ [CALL_NZ_NN]= call_nz_nn,
    /* 0xC5 */ [PUSH_BC]   = push_bc,
    /* 0xC6 */ [ADD_A_N]   = add_a_n,
    /* 0xC7 */ [RST_00H]   = rst_00,
    /* 0xC8 */ [RET_Z]     = ret_z,
    /* 0xC9 */ [RET]       = ret,
    /* 0xCA */ [JP_Z_NN]   = jp_z_nn,
    /* 0xCB */ [CB_PREFIX] = no_op,
    /* 0xCC */ [CALL_Z_NN] = call_z_nn,
    /* 0xCD */ [CALL_NN]   = call_nn,
    /* 0xCE */ [ADC_A_N]   = adc_a_n,
    /* 0xCF */ [RST_08H]   = rst_08,
    /*              ROW 14                */
    /* 0xD0 */ [RET_NC]    = ret_nc,   
    /* 0xD1 */ [POP_DE]    = pop_de,
    /* 0xD2 */ [JP_NC_NN]  = jp_nc_nn,
    /* 0xD3 */ [REG_PRINT] = print_registers,
    /* 0xD4 */ [CALL_NC_NN]= call_nc_nn,
    /* 0xD5 */ [PUSH_DE]   = push_de,
    /* 0xD6 */ [SUB_A_N]   = sub_a_n,
    /* 0xD7 */ [RST_10H]   = rst_10,
    /* 0xD8 */ [RET_C]     = ret_c,
    /* 0xD9 */ [RETI]      = reti,
    /* 0xDA */ [JP_C_NN]   = jp_c_nn,
    /* 0xDB */ [0xDB]      = no_op,
    /* 0xDC */ [CALL_C_NN] = call_c_nn,
    /* 0xDD */ [0xDD]      = no_op,
    /* 0xDE */ [SBC_A_N]   = sbc_a_n,
    /* 0xDF */ [RST_18H]   = rst_18,
    /*              ROW 15                */
    /* 0xE0 */ [LDH_A_N]   = ldh_a_n,   
    /* 0xE1 */ [POP_HL]    = pop_hl,
    /* 0xE2 */ [LDH_C_A]   = ldh_c_a,
    /* 0xE3 */ [0xE3]      = no_op,
    /* 0xE4 */ [0xE4]      = no_op,
    /* 0xE5 */ [PUSH_HL]   = push_hl,
    /* 0xE6 */ [AND_A_N]   = and_a_n,
    /* 0xE7 */ [RST_20H]   = rst_20,
    /* 0xE8 */ [ADD_SP_N]  = add_sp_n,
    /* 0xE9 */ [JP_HL]     = jp_hl,
    /* 0xEA */ [LD_NN_A]   = ld_nn_a,
    /* 0xEB */ [0xEB]      = no_op,
    /* 0xEC */ [0xEC]      = no_op,
    /* 0xED */ [0xED]      = no_op,
    /* 0xEE */ [XOR_A_N]   = xor_a_n,
    /* 0xEF */ [RST_28H]   = rst_28,
    /*              ROW 16                */
    /* 0xF0 */ [LDH_A_N]   = ldh_a_n,   
    /* 0xF1 */ [POP_AF]    = pop_af,
    /* 0xF2 */ [LD_A_C]    = ld_a_c,
    /* 0xF3 */ [DI]        = di,
    /* 0xF4 */ [0xF4]      = no_op,
    /* 0xF5 */ [PUSH_AF]   = push_af,
    /* 0xF6 */ [OR_A_N]    = or_a_n,
    /* 0xF7 */ [RST_30H]   = rst_30,
    /* 0xF8 */ [LD_HL_SP_N]= ld_hl_sp_n,
    /* 0xF9 */ [LD_SP_HL]  = ld_sp_hl,
    /* 0xFA */ [LD_A_NN]   = ld_a_nn,
    /* 0xFB */ [EI]        = ei,
    /* 0xFC */ [0xFC]      = no_op,
    /* 0xFD */ [0xFD]      = no_op,
    /* 0xFE */ [CP_A_N]    = cp_a_n,
    /* 0xFF */ [RST_38H]   = rst_38,
    /*           END OPCODES               */
};

/* ADD 1 TO IMPLICIT CYLES DUE TO PREFIX */

static void reg_rlc_8(uint8_t *r)
{ // 0x00 - 0x07
    bool c_exists = (bool) (*r & BIT_7_MASK);
    *r = (*r < 1) + c_exists;
    bool is_zero = !((bool) *r);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(c_exists, CARRY_FLAG);
}
// Implemented Function. (Z 0 0 C)
static void rlc_b()
{ //0x00
    reg_rlc_8(&(R->B));
}
static void rlc_c()
{ //0x01
    reg_rlc_8(&(R->C));
}  
static void rlc_d()
{ //0x02
    reg_rlc_8(&(R->D));
}  
static void rlc_e()
{ //0x03
    reg_rlc_8(&(R->E));
}  
static void rlc_h()
{ //0x04
    reg_rlc_8(&(R->H));
}  
static void rlc_l()
{ //0x05
    reg_rlc_8(&(R->L));
}  
static void rlc_hl()
{ //0x06
    reg_rlc_8(read_memory(getDR(HL_REG)));
    pulse_clock();
}  
static void rlc_a()
{ //0x07
    reg_rlc_8(&(R->A));
}     

static void reg_rrc_8(uint8_t *r)
{ // 0x08 - 0x0F (Z 0 0 C)
    bool c_exists = (bool) (*r & BIT_0_MASK);
    *r = (c_exists << 7) | (*r >> 1);
    bool is_zero = !((bool) *r);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(c_exists, CARRY_FLAG);
}
// Implemented Function.  (Z 0 0 C)
static void rrc_b()
{ // 0x08
    reg_rrc_8(&(R->B));
}
static void rrc_c()
{ // 0x09
    reg_rrc_8(&(R->C));
}
static void rrc_d()
{ // 0x0A
    reg_rrc_8(&(R->D));
}
static void rrc_e()
{ // 0x0B
    reg_rrc_8(&(R->E));
}
static void rrc_h()
{ // 0x0C
    reg_rrc_8(&(R->H));
}
static void rrc_l()
{ // 0x0D
    reg_rrc_8(&(R->L));
}
static void rrc_hl()
{ // 0x0E
    reg_rrc_8(read_memory(getDR(HL_REG)));
    pulse_clock();
    pulse_clock();
}
static void rrc_a()
{ // 0x0F
    reg_rrc_8(&(R->A));
}

static void reg_rl_8(uint8_t *r)
{
    bool c_exists = (bool) (*r & BIT_7_MASK);
    *r = (*r < 1) + is_flag_set(CARRY_FLAG);
    bool is_zero = !((bool) *r);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(c_exists, CARRY_FLAG);
}
// Implemented Functions. (Z 0 0 C)
static void rl_b()
{ // 0x10
    reg_rl_8(&(R->B));
}
static void rl_c()
{ // 0x11
    reg_rl_8(&(R->C));
}
static void rl_d()
{ // 0x12
    reg_rl_8(&(R->D));
}
static void rl_e()
{ // 0x13
    reg_rl_8(&(R->E));
}
static void rl_h()
{ // 0x14
    reg_rl_8(&(R->H));
}
static void rl_l()
{ // 0x15
    reg_rl_8(&(R->L));
}
static void rl_hl()
{ // 0x16
    reg_rl_8(read_memory(getDR(HL_REG)));
    pulse_clock();
}
static void rl_a()
{ // 0x17
    reg_rl_8(&(R->A));
}

static void reg_rr_8(uint8_t *r)
{
    bool c_exists = (bool) (*r & BIT_0_MASK);
    *r = (is_flag_set(CARRY_FLAG) << 7) | (*r >> 1);
    bool is_zero = !((bool) *r);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(c_exists, CARRY_FLAG);
}
// Implemented Functions. (Z 0 0 C)
static void rr_b()
{ // 0x18
    reg_rr_8(&(R->B));
}
static void rr_c()
{ // 0x19
    reg_rr_8(&(R->C));
}
static void rr_d()
{ // 0x1A
    reg_rr_8(&(R->D));
}
static void rr_e()
{ // 0x1B
    reg_rr_8(&(R->E));
}
static void rr_h()
{ // 0x1C
    reg_rr_8(&(R->H));
}
static void rr_l()
{ // 0x1D
    reg_rr_8(&(R->L));
}
static void rr_hl()
{ // 0x1E
    reg_rr_8(read_memory(getDR(HL_REG)));
    pulse_clock();
}
static void rr_a()
{ // 0x1F
    reg_rr_8(&(R->A));
}

static void reg_sla_8(uint8_t *r)
{
    bool c_exists = (*r & BIT_7_MASK) == BIT_7_MASK;
    *r = (*r << 1);
    bool is_zero = !((bool) *r);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(c_exists, CARRY_FLAG);
}
// Implemented Functions. (Z 0 0 C)
static void sla_b()
{ // 0x20
    reg_sla_8(&(R->B));
}
static void sla_c()
{ // 0x21
    reg_sla_8(&(R->C));
}
static void sla_d()
{ // 0x22
    reg_sla_8(&(R->D));
}
static void sla_e()
{ // 0x23
    reg_sla_8(&(R->E));
}
static void sla_h()
{ // 0x24
    reg_sla_8(&(R->H));
}
static void sla_l()
{ // 0x25
    reg_sla_8(&(R->L));
}
static void sla_hl()
{ // 0x26
    reg_sla_8(read_memory(getDR(HL_REG)));
}
static void sla_a()
{ // 0x27
    reg_sla_8(&(R->A));
}

static void reg_sra_8(uint8_t *r)
{
    bool c_exists = (*r & BIT_0_MASK) == BIT_0_MASK;
    *r = (*r >> 1);
    bool is_zero = !((bool) *r);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(c_exists, CARRY_FLAG);
}
// Implemented Functions. (Z 0 0 C)
static void sra_b()
{ // 0x28
    reg_sra_8(&(R->B));
}
static void sra_c()
{ // 0x29
    reg_sra_8(&(R->C));
}
static void sra_d()
{ // 0x2A
    reg_sra_8(&(R->D));
}
static void sra_e()
{ // 0x2B
    reg_sra_8(&(R->E));
}
static void sra_h()
{ // 0x2C
    reg_sra_8(&(R->H));
}
static void sra_l()
{ // 0x2D
    reg_sra_8(&(R->L));
}
static void sra_hl()
{ // 0x2E
    reg_sra_8(read_memory(getDR(HL_REG)));
}
static void sra_a()
{ // 0x2F
    reg_sra_8(&(R->A));
}

static void reg_swap_8(uint8_t *r)
{
    *r = (*r >> NIBBLE) | (*r << NIBBLE);
    bool is_zero = !((bool) *r);
    set_flag(is_zero, ZERO_FLAG);
}
// Implemented Functions. (Z 0 0 0)
static void swap_b()
{ // 0x30
    reg_swap_8(&(R->B));
}
static void swap_c()
{ // 0x31
    reg_swap_8(&(R->C));   
}
static void swap_d()
{ // 0x32
    reg_swap_8(&(R->D));
}
static void swap_e()
{ // 0x33
    reg_swap_8(&(R->E));
}
static void swap_h()
{ // 0x34
    reg_swap_8(&(R->H));
}
static void swap_l()
{ // 0x35
    reg_swap_8(&(R->L));
}
static void swap_hl()
{ // 0x36
    reg_swap_8(read_memory(getDR(HL_REG)));
}
static void swap_a()
{ // 0x37
    reg_swap_8(&(R->A));
}

static void reg_srl_8(uint8_t *r)
{
    bool c_exists = (*r & BIT_0_MASK) == BIT_0_MASK;
    *r = (*r >> 1);
    bool is_zero = !((bool) *r);
    set_flag(is_zero, ZERO_FLAG);
    set_flag(c_exists, CARRY_FLAG);
}
// Implemented Functions. (Z 0 0 C)
static void srl_b()
{ // 0x38
    reg_srl_8(&(R->B));
}
static void srl_c()
{ // 0x39
    reg_srl_8(&(R->C));
}
static void srl_d()
{ // 0x3A
    reg_srl_8(&(R->D));
}
static void srl_e()
{ // 0x3B
    reg_srl_8(&(R->E));
}
static void srl_h()
{ // 0x3C
    reg_srl_8(&(R->H));
}
static void srl_l()
{ // 0x3D
    reg_srl_8(&(R->L));
}
static void srl_hl()
{ // 0x3E
    reg_srl_8(read_memory(getDR(HL_REG)));
}
static void srl_a()
{ // 0x3F
    reg_srl_8(&(R->A));
}

static void reg_bit_x(BitMask mask, uint8_t *r)
{
    bool is_zero = (*r & mask) == mask;
    set_flag(is_zero, ZERO_FLAG);
    set_flag(false, SUBTRACT_FLAG);
    set_flag(true, HALF_CARRY_FLAG);
}
// Implemented Functions. (Z 0 1 -)
static void bit_0_b()
{ // 0x40
    reg_bit_x(BIT_0_MASK, &(R->B));
}
static void bit_0_c()
{ // 0x41
    reg_bit_x(BIT_0_MASK, &(R->C));
}
static void bit_0_d()
{ // 0x42
    reg_bit_x(BIT_0_MASK, &(R->D));
}
static void bit_0_e()
{ // 0x43
    reg_bit_x(BIT_0_MASK, &(R->E));
}
static void bit_0_h()
{ // 0x44
    reg_bit_x(BIT_0_MASK, &(R->H));
}
static void bit_0_l()
{ // 0x45
    reg_bit_x(BIT_0_MASK, &(R->L));
}
static void bit_0_hl()
{ // 0x46
    reg_bit_x(BIT_0_MASK, read_memory(getDR(HL_REG)));
}
static void bit_0_a()
{ // 0x47
    reg_bit_x(BIT_0_MASK, &(R->A));
}
static void bit_1_b()
{ // 0x48
    reg_bit_x(BIT_1_MASK, &(R->B));
}
static void bit_1_c()
{ // 0x49
    reg_bit_x(BIT_1_MASK, &(R->C));
}
static void bit_1_d()
{ // 0x4A
    reg_bit_x(BIT_1_MASK, &(R->D));
}
static void bit_1_e()
{ // 0x4B
    reg_bit_x(BIT_1_MASK, &(R->E));
}
static void bit_1_h()
{ // 0x4C
    reg_bit_x(BIT_1_MASK, &(R->H));
}
static void bit_1_l()
{ // 0x4D
    reg_bit_x(BIT_1_MASK, &(R->L));
}
static void bit_1_hl()
{ // 0x4E
    reg_bit_x(BIT_1_MASK, read_memory(getDR(HL_REG)));
}
static void bit_1_a()
{ // 0x4F
    reg_bit_x(BIT_1_MASK, &(R->A));
}
static void bit_2_b()
{ // 0x50
    reg_bit_x(BIT_2_MASK, &(R->B));
}
static void bit_2_c()
{ // 0x51
    reg_bit_x(BIT_2_MASK, &(R->C));
}
static void bit_2_d()
{ // 0x52
    reg_bit_x(BIT_2_MASK, &(R->D));
}
static void bit_2_e()
{ // 0x53
    reg_bit_x(BIT_2_MASK, &(R->E));
}
static void bit_2_h()
{ // 0x54
    reg_bit_x(BIT_2_MASK, &(R->H));
}
static void bit_2_l()
{ // 0x55
    reg_bit_x(BIT_2_MASK, &(R->L));
}
static void bit_2_hl()
{ // 0x56
    reg_bit_x(BIT_2_MASK, read_memory(getDR(HL_REG)));
}
static void bit_2_a()
{ // 0x57
    reg_bit_x(BIT_2_MASK, &(R->A));
}
static void bit_3_b()
{ // 0x58
    reg_bit_x(BIT_3_MASK, &(R->B));
}
static void bit_3_c()
{ // 0x59
    reg_bit_x(BIT_3_MASK, &(R->C));
}
static void bit_3_d()
{ // 0x5A
    reg_bit_x(BIT_3_MASK, &(R->D));
}
static void bit_3_e()
{ // 0x5B
    reg_bit_x(BIT_3_MASK, &(R->E));
}
static void bit_3_h()
{ // 0x5C
    reg_bit_x(BIT_3_MASK, &(R->H));
}
static void bit_3_l()
{ // 0x5D
    reg_bit_x(BIT_3_MASK, &(R->L));
}
static void bit_3_hl()
{ // 0x5E
    reg_bit_x(BIT_3_MASK, read_memory(getDR(HL_REG)));
}
static void bit_3_a()
{ // 0x5F
    reg_bit_x(BIT_3_MASK, &(R->A));
}
static void bit_4_b()
{ // 0x60
    reg_bit_x(BIT_4_MASK, &(R->B));
}
static void bit_4_c()
{ // 0x61
    reg_bit_x(BIT_4_MASK, &(R->C));
}
static void bit_4_d()
{ // 0x62
    reg_bit_x(BIT_4_MASK, &(R->D));
}
static void bit_4_e()
{ // 0x63
    reg_bit_x(BIT_4_MASK, &(R->E));
}
static void bit_4_h()
{ // 0x64
    reg_bit_x(BIT_4_MASK, &(R->H));
}
static void bit_4_l()
{ // 0x65
    reg_bit_x(BIT_4_MASK, &(R->L));
}
static void bit_4_hl()
{ // 0x66
    reg_bit_x(BIT_4_MASK, read_memory(getDR(HL_REG)));
}
static void bit_4_a()
{ // 0x67
    reg_bit_x(BIT_4_MASK, &(R->A));
}
static void bit_5_b()
{ // 0x68
    reg_bit_x(BIT_5_MASK, &(R->B));
}
static void bit_5_c()
{ // 0x69
    reg_bit_x(BIT_5_MASK, &(R->C));
}
static void bit_5_d()
{ // 0x6A
    reg_bit_x(BIT_5_MASK, &(R->D));
}
static void bit_5_e()
{ // 0x6B
    reg_bit_x(BIT_5_MASK, &(R->E));
}
static void bit_5_h()
{ // 0x6C
    reg_bit_x(BIT_5_MASK, &(R->H));
}
static void bit_5_l()
{ // 0x6D
    reg_bit_x(BIT_5_MASK, &(R->L));
}
static void bit_5_hl()
{ // 0x6E
    reg_bit_x(BIT_5_MASK, read_memory(getDR(HL_REG)));
}
static void bit_5_a()
{ // 0x6F
    reg_bit_x(BIT_5_MASK, &(R->A));
}
static void bit_6_b()
{ // 0x70
    reg_bit_x(BIT_6_MASK, &(R->B));
}
static void bit_6_c()
{ // 0x71
    reg_bit_x(BIT_6_MASK, &(R->C));
}
static void bit_6_d()
{ // 0x72
    reg_bit_x(BIT_6_MASK, &(R->D));
}
static void bit_6_e()
{ // 0x73
    reg_bit_x(BIT_6_MASK, &(R->E));
}
static void bit_6_h()
{ // 0x74
    reg_bit_x(BIT_6_MASK, &(R->H));
}
static void bit_6_l()
{ // 0x75
    reg_bit_x(BIT_6_MASK, &(R->L));
}
static void bit_6_hl()
{ // 0x76
    reg_bit_x(BIT_6_MASK, read_memory(getDR(HL_REG)));
}
static void bit_6_a()
{ // 0x77
    reg_bit_x(BIT_6_MASK, &(R->A));
}
static void bit_7_b()
{ // 0x78
    reg_bit_x(BIT_7_MASK, &(R->B));
}
static void bit_7_c()
{ // 0x79
    reg_bit_x(BIT_7_MASK, &(R->C));
}
static void bit_7_d()
{ // 0x7A
    reg_bit_x(BIT_7_MASK, &(R->D));
}
static void bit_7_e()
{ // 0x7B
    reg_bit_x(BIT_7_MASK, &(R->E));
}
static void bit_7_h()
{ // 0x7C
    reg_bit_x(BIT_7_MASK, &(R->H));
}
static void bit_7_l()
{ // 0x7D
    reg_bit_x(BIT_7_MASK, &(R->L));
}
static void bit_7_hl()
{ // 0x7E
    reg_bit_x(BIT_7_MASK, read_memory(getDR(HL_REG)));
}
static void bit_7_a()
{ // 0x7F
    reg_bit_x(BIT_7_MASK, &(R->A));
}

static void res_0_b()
{ // 0x80
    R->B = (R->B & ~BIT_0_MASK);
}
static void res_0_c()
{ // 0x81
    R->C = (R->C & ~BIT_0_MASK);
}
static void res_0_d()
{ // 0x82
    R->D = (R->D & ~BIT_0_MASK);
}
static void res_0_e()
{ // 0x83
    R->E = (R->E & ~BIT_0_MASK);
}
static void res_0_h()
{ // 0x84
    R->H = (R->H & ~BIT_0_MASK);
}
static void res_0_l()
{ // 0x85
    R->L = (R->L & ~BIT_0_MASK);
}
static void res_0_hl()
{ // 0x86
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory & ~BIT_0_MASK));
}
static void res_0_a()
{ // 0x87
    R->A = (R->A & ~BIT_0_MASK);
}
static void res_1_b()
{ // 0x88
    R->B = (R->B & ~BIT_1_MASK);
}
static void res_1_c()
{ // 0x89
    R->C = (R->C & ~BIT_1_MASK);
}
static void res_1_d()
{ // 0x8A
    R->D = (R->D & ~BIT_1_MASK);
}
static void res_1_e()
{ // 0x8B
    R->E = (R->E & ~BIT_1_MASK);
}
static void res_1_h()
{ // 0x8C
    R->H = (R->H & ~BIT_1_MASK);
}
static void res_1_l()
{ // 0x8D
    R->L = (R->L & ~BIT_1_MASK);
}
static void res_1_hl()
{ // 0x8E
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory & ~BIT_1_MASK));
}
static void res_1_a()
{ // 0x8F
    R->A = (R->A & ~BIT_1_MASK);
}
static void res_2_b()
{ // 0x90
    R->B = (R->B & ~BIT_2_MASK);
}
static void res_2_c()
{ // 0x91
    R->C = (R->C & ~BIT_2_MASK);
}
static void res_2_d()
{ // 0x92
    R->D = (R->D & ~BIT_2_MASK);
}
static void res_2_e()
{ // 0x93
    R->E = (R->E & ~BIT_2_MASK);
}
static void res_2_h()
{ // 0x94
    R->H = (R->H & ~BIT_2_MASK);
}
static void res_2_l()
{ // 0x95
    R->L = (R->L & ~BIT_2_MASK);
}
static void res_2_hl()
{ // 0x96
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory & ~BIT_2_MASK));
}
static void res_2_a()
{ // 0x97
    R->A = (R->A & ~BIT_2_MASK);
}
static void res_3_b()
{ // 0x98
    R->B = (R->B & ~BIT_3_MASK);
}
static void res_3_c()
{ // 0x99
    R->C = (R->C & ~BIT_3_MASK);
}
static void res_3_d()
{ // 0x9A
    R->D = (R->D & ~BIT_3_MASK);
}
static void res_3_e()
{ // 0x9B
    R->E = (R->E & ~BIT_3_MASK);
}
static void res_3_h()
{ // 0x9C
    R->H = (R->H & ~BIT_3_MASK);
}
static void res_3_l()
{ // 0x9D
    R->L = (R->L & ~BIT_3_MASK);
}
static void res_3_hl()
{ // 0x9E
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory & ~BIT_3_MASK));
}
static void res_3_a()
{ // 0x9F
    R->A = (R->A & ~BIT_3_MASK);
}
static void res_4_b()
{ // 0xA0
    R->B = (R->B & ~BIT_4_MASK);
}
static void res_4_c()
{ // 0xA1
    R->C = (R->C & ~BIT_4_MASK);
}
static void res_4_d()
{ // 0xA2
    R->D = (R->D & ~BIT_4_MASK);
}
static void res_4_e()
{ // 0xA3
    R->E = (R->E & ~BIT_4_MASK);
}
static void res_4_h()
{ // 0xA4
    R->H = (R->H & ~BIT_4_MASK);
}
static void res_4_l()
{ // 0xA5
    R->L = (R->L & ~BIT_4_MASK);
}
static void res_4_hl()
{ // 0xA6
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory & ~BIT_4_MASK));
}
static void res_4_a()
{ // 0xA7
    R->A = (R->A & ~BIT_4_MASK);
}
static void res_5_b()
{ // 0xA8
    R->B = (R->B & ~BIT_5_MASK);
}
static void res_5_c()
{ // 0xA9
    R->C = (R->C & ~BIT_5_MASK);
}
static void res_5_d()
{ // 0xAA
    R->D = (R->D & ~BIT_5_MASK);
}
static void res_5_e()
{ // 0xAB
    R->E = (R->E & ~BIT_5_MASK);
}
static void res_5_h()
{ // 0xAC
    R->H = (R->H & ~BIT_5_MASK);
}
static void res_5_l()
{ // 0xAD
    R->L = (R->L & ~BIT_5_MASK);
}
static void res_5_hl()
{ // 0xAE
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory & ~BIT_5_MASK));
}
static void res_5_a()
{ // 0xAF
    R->A = (R->A & ~BIT_5_MASK);
}
static void res_6_b()
{ // 0xB0
    R->B = (R->B & ~BIT_6_MASK);
}
static void res_6_c()
{ // 0xB1
    R->C = (R->C & ~BIT_6_MASK);
}
static void res_6_d()
{ // 0xB2
    R->D = (R->D & ~BIT_6_MASK);
}
static void res_6_e()
{ // 0xB3
    R->E = (R->E & ~BIT_6_MASK);
}
static void res_6_h()
{ // 0xB4
    R->H = (R->H & ~BIT_6_MASK);
}
static void res_6_l()
{ // 0xB5
    R->L = (R->L & ~BIT_6_MASK);
}
static void res_6_hl()
{ // 0xB6
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory & ~BIT_6_MASK));
}
static void res_6_a()
{ // 0xB7
    R->A = (R->A & ~BIT_6_MASK);
}
static void res_7_b()
{ // 0xB8
    R->B = (R->B & ~BIT_7_MASK);
}
static void res_7_c()
{ // 0xB9
    R->C = (R->C & ~BIT_7_MASK);
}
static void res_7_d()
{ // 0xBA
    R->D = (R->D & ~BIT_7_MASK);
}
static void res_7_e()
{ // 0xBB
    R->E = (R->E & ~BIT_7_MASK);
}
static void res_7_h()
{ // 0xBC
    R->H = (R->H & ~BIT_7_MASK);
}
static void res_7_l()
{ // 0xBD
    R->L = (R->L & ~BIT_7_MASK);
}
static void res_7_hl()
{ // 0xBE
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory & ~BIT_7_MASK));
}
static void res_7_a()
{ // 0xBF
    R->A = (R->A & ~BIT_7_MASK);
}

static void set_0_b()
{ // 0xC0
    R->B = (R->B | BIT_0_MASK);
}
static void set_0_c()
{ // 0xC1
    R->C = (R->C | BIT_0_MASK);
}
static void set_0_d()
{ // 0xC2
    R->D = (R->D | BIT_0_MASK);
}
static void set_0_e()
{ // 0xC3
    R->E = (R->E | BIT_0_MASK);
}
static void set_0_h()
{ // 0xC4
    R->H = (R->H | BIT_0_MASK);
}
static void set_0_l()
{ // 0xDD
    R->L = (R->L | BIT_1_MASK);
}
static void set_0_hl()
{ // 0xC5
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory | BIT_0_MASK));
}
static void set_0_a()
{ // 0xC7
    R->A = (R->A | BIT_0_MASK);
}
static void set_1_b()
{ // 0xC8
    R->B = (R->B | BIT_1_MASK);
}
static void set_1_c()
{ // 0xC9
    R->C = (R->C | BIT_1_MASK);
}
static void set_1_d()
{ // 0xCA
    R->D = (R->D | BIT_1_MASK);
}
static void set_1_e()
{ // 0xCB
    R->E = (R->E | BIT_1_MASK);
}
static void set_1_h()
{ // 0xCC
    R->H = (R->H | BIT_1_MASK);
}
static void set_1_l()
{ // 0xCD
    R->L = (R->L | BIT_1_MASK);
}
static void set_1_hl()
{ // 0xCE
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory | BIT_1_MASK));
}
static void set_1_a()
{ // 0xCF
    R->A = (R->A | BIT_1_MASK);
}
static void set_2_b()
{ // 0xC0
    R->B = (R->B | BIT_2_MASK);
}
static void set_2_c()
{ // 0xC1
    R->C = (R->C | BIT_2_MASK);
}
static void set_2_d()
{ // 0xC2
    R->D = (R->D | BIT_2_MASK);
}
static void set_2_e()
{ // 0xC3
    R->E = (R->E | BIT_2_MASK);
}
static void set_2_h()
{ // 0xC4
    R->H = (R->H | BIT_2_MASK);
}
static void set_2_l()
{ // 0xC5
    R->L = (R->L | BIT_2_MASK);
}
static void set_2_hl()
{ // 0xC6
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory | BIT_2_MASK));
}
static void set_2_a()
{ // 0xC7
    R->A = (R->A | BIT_2_MASK);
}
static void set_3_b()
{ // 0xC8
    R->B = (R->B | BIT_3_MASK);
}
static void set_3_c()
{ // 0xC9
    R->C = (R->C | BIT_3_MASK);
}
static void set_3_d()
{ // 0xCA
    R->D = (R->D | BIT_3_MASK);
}
static void set_3_e()
{ // 0xCB
    R->E = (R->E | BIT_3_MASK);
}
static void set_3_h()
{ // 0xCC
    R->H = (R->H | BIT_3_MASK);
}
static void set_3_l()
{ // 0xCD
    R->L = (R->L | BIT_3_MASK);
}
static void set_3_hl()
{ // 0xCE
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory | BIT_3_MASK));
}
static void set_3_a()
{ // 0xCF
    R->A = (R->A | BIT_3_MASK);
}
static void set_4_b()
{ // 0xD0
    R->B = (R->B | BIT_4_MASK);
}
static void set_4_c()
{ // 0xD1
    R->C = (R->C | BIT_4_MASK);
}
static void set_4_d()
{ // 0xD2
    R->D = (R->D | BIT_4_MASK);
}
static void set_4_e()
{ // 0xD3
    R->E = (R->E | BIT_4_MASK);
}
static void set_4_h()
{ // 0xD4
    R->H = (R->H | BIT_4_MASK);
}
static void set_4_l()
{ // 0xD5
    R->L = (R->L | BIT_4_MASK);
}
static void set_4_hl()
{ // 0xD6
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory | BIT_4_MASK));
}
static void set_4_a()
{ // 0xD7
    R->A = (R->A | BIT_4_MASK);
}
static void set_5_b()
{ // 0xD8
    R->B = (R->B | BIT_5_MASK);
}
static void set_5_c()
{ // 0xD9
    R->C = (R->C | BIT_5_MASK);
}
static void set_5_d()
{ // 0xDA
    R->D = (R->D | BIT_5_MASK);
}
static void set_5_e()
{ // 0xDB
    R->E = (R->E | BIT_5_MASK);
}
static void set_5_h()
{ // 0xDC
    R->H = (R->H | BIT_5_MASK);
}
static void set_5_l()
{ // 0xDD
    R->L = (R->L | BIT_5_MASK);
}
static void set_5_hl()
{ // 0xDE
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory | BIT_5_MASK));
}
static void set_5_a()
{ // 0xDF
    R->A = (R->A | BIT_5_MASK);
}
static void set_6_b()
{ // 0xF0
    R->B = (R->B | BIT_5_MASK);
}
static void set_6_c()
{ // 0xF1
    R->C = (R->C | BIT_6_MASK);
}
static void set_6_d()
{ // 0xF2
    R->D = (R->D | BIT_6_MASK);
}
static void set_6_e()
{ // 0xF3
    R->E = (R->E | BIT_6_MASK);
}
static void set_6_h()
{ // 0xF4
    R->H = (R->H | BIT_6_MASK);
}
static void set_6_l()
{ // 0xF5
    R->L = (R->L | BIT_6_MASK);
}
static void set_6_hl()
{ // 0xF6
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory | BIT_6_MASK));
}
static void set_6_a()
{ // 0xF7
    R->A = (R->A | BIT_6_MASK);
}
static void set_7_b()
{ // 0xF8
    R->B = (R->B | BIT_7_MASK);
}
static void set_7_c()
{ // 0xF9
    R->C = (R->C | BIT_7_MASK);
}
static void set_7_d()
{ // 0xFA
    R->D = (R->D | BIT_7_MASK);
}
static void set_7_e()
{ // 0xFB
    R->E = (R->E | BIT_7_MASK);
}
static void set_7_h()
{ // 0xFC
    R->H = (R->H | BIT_7_MASK);
}
static void set_7_l()
{ // 0xFD
    R->L = (R->L | BIT_7_MASK);
}
static void set_7_hl()
{ // 0xFE
    uint8_t *memory = read_memory(getDR(HL_REG));
    write_memory(getDR(HL_REG), (*memory | BIT_7_MASK));
}
static void set_7_a()
{ // 0xFF
    R->A = (R->A | BIT_7_MASK);
}

OpcodeHandler prefix_opcode_table[256] = 
{
    /*              ROW 1               */
    /* 0x00 */ [RLC_B]     = rlc_b,   
    /* 0x01 */ [RLC_C]     = rlc_c,
    /* 0x02 */ [RLC_D]     = rlc_d,
    /* 0x03 */ [RLC_E]     = rlc_e,
    /* 0x04 */ [RLC_H]     = rlc_h,
    /* 0x05 */ [RLC_L]     = rlc_l,
    /* 0x06 */ [RLC_HL]    = rlc_hl,
    /* 0x07 */ [RLC_A]     = rlc_a,
    /* 0x08 */ [RRC_B]     = rrc_b,
    /* 0x09 */ [RRC_C]     = rrc_c,
    /* 0x0A */ [RRC_D]     = rrc_d,
    /* 0x0B */ [RRC_E]     = rrc_e,
    /* 0x0C */ [RRC_H]     = rrc_h,
    /* 0x0D */ [RRC_L]     = rrc_l,
    /* 0x0E */ [RRC_HL]    = rrc_hl,
    /* 0x0F */ [RRC_A]     = rrc_a,
    /*              ROW 2                */
    /* 0x10 */ [RL_B]      = rl_b,   
    /* 0x11 */ [RL_C]      = rl_c,
    /* 0x12 */ [RL_D]      = rl_d,
    /* 0x13 */ [RL_E]      = rl_e,
    /* 0x14 */ [RL_H]      = rl_h,
    /* 0x15 */ [RL_L]      = rl_l,
    /* 0x16 */ [RL_HL]     = rl_hl,
    /* 0x17 */ [RL_A]      = rr_a,
    /* 0x18 */ [RR_B]      = rr_b,
    /* 0x19 */ [RR_C]      = rr_c,
    /* 0x1A */ [RR_D]      = rr_d,
    /* 0x1B */ [RR_E]      = rr_e,
    /* 0x1C */ [RR_H]      = rr_h,
    /* 0x1D */ [RR_L]      = rr_l,
    /* 0x1E */ [RR_HL]     = rr_hl,
    /* 0x1F */ [RR_A]      = rr_a,
    /*              ROW 3                */
    /* 0x20 */ [SLA_B]     = sla_b,   
    /* 0x21 */ [SLA_C]     = sla_c,
    /* 0x22 */ [SLA_D]     = sla_d,
    /* 0x23 */ [SLA_E]     = sla_e,
    /* 0x24 */ [SLA_H]     = sla_h,
    /* 0x25 */ [SLA_L]     = sla_l,
    /* 0x26 */ [SLA_HL]    = sla_hl,
    /* 0x27 */ [SLA_A]     = sla_a,
    /* 0x28 */ [SRA_B]     = sra_b,
    /* 0x29 */ [SRA_C]     = sra_c,
    /* 0x2A */ [SRA_D]     = sra_d,
    /* 0x2B */ [SRA_E]     = sra_e,
    /* 0x2C */ [SRA_H]     = sra_h,
    /* 0x2D */ [SRA_L]     = sra_l,
    /* 0x2E */ [SRA_HL]    = sra_hl,
    /* 0x2F */ [SRA_A]     = sra_a,
    /*              ROW 4                */
    /* 0x30 */ [SWAP_B]    = swap_b,   
    /* 0x31 */ [SWAP_C]    = swap_c,
    /* 0x32 */ [SWAP_D]    = swap_d,
    /* 0x33 */ [SWAP_E]    = swap_e,
    /* 0x34 */ [SWAP_H]    = swap_h,
    /* 0x35 */ [SWAP_L]    = swap_l,
    /* 0x36 */ [SWAP_HL]   = swap_hl,
    /* 0x37 */ [SWAP_A]    = swap_a,
    /* 0x38 */ [SRL_B]     = srl_b,
    /* 0x39 */ [SRL_C]     = srl_c,
    /* 0x3A */ [SRL_D]     = srl_d,
    /* 0x3B */ [SRL_E]     = srl_e,
    /* 0x3C */ [SRL_H]     = srl_h,
    /* 0x3D */ [SRL_L]     = srl_l,
    /* 0x3E */ [SRL_HL]    = srl_hl,
    /* 0x3F */ [SRL_A]     = srl_a,
    /*              ROW 5                */
    /* 0x40 */ [BIT_0_B]   = bit_0_b,   
    /* 0x41 */ [BIT_0_C]   = bit_0_c,
    /* 0x42 */ [BIT_0_D]   = bit_0_d,
    /* 0x43 */ [BIT_0_E]   = bit_0_e,
    /* 0x44 */ [BIT_0_H]   = bit_0_h,
    /* 0x45 */ [BIT_0_L]   = bit_0_l,
    /* 0x46 */ [BIT_0_HL]  = bit_0_hl,
    /* 0x47 */ [BIT_0_A]   = bit_0_a,
    /* 0x48 */ [BIT_1_B]   = bit_1_b,
    /* 0x49 */ [BIT_1_C]   = bit_1_c,
    /* 0x4A */ [BIT_1_D]   = bit_1_d,
    /* 0x4B */ [BIT_1_E]   = bit_1_e,
    /* 0x4C */ [BIT_1_H]   = bit_1_h,
    /* 0x4D */ [BIT_1_L]   = bit_1_l,
    /* 0x4E */ [BIT_1_HL]  = bit_1_hl,
    /* 0x4F */ [BIT_1_A]   = bit_1_a,
    /*              ROW 6                */
    /* 0x50 */ [BIT_2_B]   = bit_2_b,   
    /* 0x51 */ [BIT_2_C]   = bit_2_c,
    /* 0x52 */ [BIT_2_D]   = bit_2_d,
    /* 0x53 */ [BIT_2_E]   = bit_2_e,
    /* 0x54 */ [BIT_2_H]   = bit_2_h,
    /* 0x55 */ [BIT_2_L]   = bit_2_l,
    /* 0x56 */ [BIT_2_HL]  = bit_2_hl,
    /* 0x57 */ [BIT_2_A]   = bit_2_a,
    /* 0x58 */ [BIT_3_B]   = bit_3_b,
    /* 0x59 */ [BIT_3_C]   = bit_3_c,
    /* 0x5A */ [BIT_3_D]   = bit_3_d,
    /* 0x5B */ [BIT_3_E]   = bit_3_e,
    /* 0x5C */ [BIT_3_H]   = bit_3_h,
    /* 0x5D */ [BIT_3_L]   = bit_3_l,
    /* 0x5E */ [BIT_3_HL]  = bit_3_hl,
    /* 0x5F */ [BIT_3_A]   = bit_3_a,
    /*              ROW 7                */
    /* 0x60 */ [BIT_4_B]   = bit_4_b,   
    /* 0x61 */ [BIT_4_C]   = bit_4_c,
    /* 0x62 */ [BIT_4_D]   = bit_4_d,
    /* 0x63 */ [BIT_4_E]   = bit_4_e,
    /* 0x64 */ [BIT_4_H]   = bit_4_h,
    /* 0x65 */ [BIT_4_L]   = bit_4_l,
    /* 0x66 */ [BIT_4_HL]  = bit_4_hl,
    /* 0x67 */ [BIT_4_A]   = bit_4_a,
    /* 0x68 */ [BIT_5_B]   = bit_5_b,
    /* 0x69 */ [BIT_5_C]   = bit_5_c,
    /* 0x6A */ [BIT_5_D]   = bit_5_d,
    /* 0x6B */ [BIT_5_E]   = bit_5_e,
    /* 0x6C */ [BIT_5_H]   = bit_5_h,
    /* 0x6D */ [BIT_5_L]   = bit_5_l,
    /* 0x6E */ [BIT_5_HL]  = bit_5_hl,
    /* 0x6F */ [BIT_5_A]   = bit_5_a,
    /*              ROW 8                */
    /* 0x70 */ [BIT_6_B]   = bit_7_b,   
    /* 0x71 */ [BIT_6_C]   = bit_7_c,
    /* 0x72 */ [BIT_6_D]   = bit_7_d,
    /* 0x73 */ [BIT_6_E]   = bit_7_e,
    /* 0x74 */ [BIT_6_H]   = bit_7_h,
    /* 0x75 */ [BIT_6_L]   = bit_7_l,
    /* 0x76 */ [BIT_6_HL]  = bit_7_hl,
    /* 0x77 */ [BIT_6_A]   = bit_7_a,
    /* 0x78 */ [BIT_7_B]   = bit_7_b,
    /* 0x79 */ [BIT_7_C]   = bit_7_c,
    /* 0x7A */ [BIT_7_D]   = bit_7_d,
    /* 0x7B */ [BIT_7_E]   = bit_7_e,
    /* 0x7C */ [BIT_7_H]   = bit_7_h,
    /* 0x7D */ [BIT_7_L]   = bit_7_l,
    /* 0x7E */ [BIT_7_HL]  = bit_7_hl,
    /* 0x7F */ [BIT_7_A]   = bit_7_a,
    /*              ROW 9                */
    /* 0x80 */ [RES_0_B]   = res_0_b,   
    /* 0x81 */ [RES_0_C]   = res_0_c,
    /* 0x82 */ [RES_0_D]   = res_0_d,
    /* 0x83 */ [RES_0_E]   = res_0_e,
    /* 0x84 */ [RES_0_H]   = res_0_h,
    /* 0x85 */ [RES_0_L]   = res_0_l,
    /* 0x86 */ [RES_0_HL]  = res_0_hl,
    /* 0x87 */ [RES_0_A]   = res_0_a,
    /* 0x88 */ [RES_1_B]   = res_1_b,
    /* 0x89 */ [RES_1_C]   = res_1_c,
    /* 0x8A */ [RES_1_D]   = res_1_d,
    /* 0x8B */ [RES_1_E]   = res_1_e,
    /* 0x8C */ [RES_1_H]   = res_1_h,
    /* 0x8D */ [RES_1_L]   = res_1_l,
    /* 0x8E */ [RES_1_HL]  = res_1_hl,
    /* 0x8F */ [RES_1_A]   = res_1_a,
    /*              ROW 10                */
    /* 0x90 */ [RES_2_B]   = res_2_b,   
    /* 0x91 */ [RES_2_C]   = res_2_c,
    /* 0x92 */ [RES_2_D]   = res_2_d,
    /* 0x93 */ [RES_2_E]   = res_2_e,
    /* 0x94 */ [RES_2_H]   = res_2_h,
    /* 0x95 */ [RES_2_L]   = res_2_l,
    /* 0x96 */ [RES_2_HL]  = res_2_hl,
    /* 0x97 */ [RES_2_A]   = res_2_a,
    /* 0x98 */ [RES_3_B]   = res_3_b,
    /* 0x99 */ [RES_3_C]   = res_3_c,
    /* 0x9A */ [RES_3_D]   = res_3_d,
    /* 0x9B */ [RES_3_E]   = res_3_e,
    /* 0x9C */ [RES_3_H]   = res_3_h,
    /* 0x9D */ [RES_3_L]   = res_3_l,
    /* 0x9E */ [RES_3_HL]  = res_3_hl,
    /* 0x9F */ [RES_3_A]   = res_3_a,
    /*              ROW 10                */
    /* 0xA0 */ [RES_4_B]   = res_4_b,   
    /* 0xA1 */ [RES_4_C]   = res_4_c,
    /* 0xA2 */ [RES_4_D]   = res_4_d,
    /* 0xA3 */ [RES_4_E]   = res_4_e,
    /* 0xA4 */ [RES_4_H]   = res_4_h,
    /* 0xA5 */ [RES_4_L]   = res_4_l,
    /* 0xA6 */ [RES_4_HL]  = res_4_hl,
    /* 0xA7 */ [RES_4_A]   = res_4_a,
    /* 0xA8 */ [RES_5_B]   = res_5_b,
    /* 0xA9 */ [RES_5_C]   = res_5_c,
    /* 0xAA */ [RES_5_D]   = res_5_d,
    /* 0xAB */ [RES_5_E]   = res_5_e,
    /* 0xAC */ [RES_5_H]   = res_5_h,
    /* 0xAD */ [RES_5_L]   = res_5_l,
    /* 0xAE */ [RES_5_HL]  = res_5_hl,
    /* 0xAF */ [RES_5_A]   = res_5_a,
    /*              ROW 11                */
    /* 0xB0 */ [RES_6_B]   = res_6_b,   
    /* 0xB1 */ [RES_6_C]   = res_6_c,
    /* 0xB2 */ [RES_6_D]   = res_6_d,
    /* 0xB3 */ [RES_6_E]   = res_6_e,
    /* 0xB4 */ [RES_6_H]   = res_6_l,
    /* 0xB5 */ [RES_6_L]   = res_6_h,
    /* 0xB6 */ [RES_6_HL]  = res_6_hl,
    /* 0xB7 */ [RES_6_A]   = res_6_a,
    /* 0xB8 */ [RES_7_B]   = res_6_b,
    /* 0xB9 */ [RES_7_C]   = res_7_c,
    /* 0xBA */ [RES_7_D]   = res_7_d,
    /* 0xBB */ [RES_7_E]   = res_7_e,
    /* 0xBC */ [RES_7_H]   = res_7_h,
    /* 0xBD */ [RES_7_L]   = res_7_l,
    /* 0xBE */ [RES_7_HL]  = res_7_hl,
    /* 0xBF */ [RES_7_A]   = res_7_a,
    /*              ROW 12                */
    /* 0xC0 */ [SET_0_B]   = set_0_b,   
    /* 0xC1 */ [SET_0_C]   = set_0_c,
    /* 0xC2 */ [SET_0_D]   = set_0_d,
    /* 0xC3 */ [SET_0_E]   = set_0_e,
    /* 0xC4 */ [SET_0_H]   = set_0_h,
    /* 0xC5 */ [SET_0_L]   = set_0_l,
    /* 0xC6 */ [SET_0_HL]  = set_0_hl,
    /* 0xC7 */ [SET_0_A]   = set_0_a,
    /* 0xC8 */ [SET_1_B]   = set_1_b,
    /* 0xC9 */ [SET_1_C]   = set_1_c,
    /* 0xCA */ [SET_1_D]   = set_1_d,
    /* 0xCB */ [SET_1_E]   = set_1_e,
    /* 0xCC */ [SET_1_H]   = set_1_h,
    /* 0xCD */ [SET_1_L]   = set_1_l,
    /* 0xCE */ [SET_1_HL]  = set_1_hl,
    /* 0xCF */ [SET_1_A]   = set_1_a,
    /*              ROW 13                */
    /* 0xD0 */ [SET_2_B]   = set_2_b,   
    /* 0xD1 */ [SET_2_C]   = set_2_c,
    /* 0xD2 */ [SET_2_D]   = set_2_d,
    /* 0xD3 */ [SET_2_E]   = set_2_e,
    /* 0xD4 */ [SET_2_H]   = set_2_h,
    /* 0xD5 */ [SET_2_L]   = set_2_l,
    /* 0xD6 */ [SET_2_HL]  = set_2_hl,
    /* 0xD7 */ [SET_2_A]   = set_2_a,
    /* 0xD8 */ [SET_3_B]   = set_3_b,
    /* 0xD9 */ [SET_3_C]   = set_3_c,
    /* 0xDA */ [SET_3_D]   = set_3_d,
    /* 0xDB */ [SET_3_E]   = set_3_e,
    /* 0xDC */ [SET_3_H]   = set_3_h,
    /* 0xDD */ [SET_3_L]   = set_3_l,
    /* 0xDE */ [SET_3_HL]  = set_3_hl,
    /* 0xDF */ [SET_3_A]   = set_3_a,
    /*              ROW 14                */
    /* 0xE0 */ [SET_4_B]   = set_4_b,   
    /* 0xE1 */ [SET_4_C]   = set_4_c,
    /* 0xE2 */ [SET_4_D]   = set_4_d,
    /* 0xE3 */ [SET_4_E]   = set_4_e,
    /* 0xE4 */ [SET_4_H]   = set_4_h,
    /* 0xE5 */ [SET_4_L]   = set_4_l,
    /* 0xE6 */ [SET_4_HL]  = set_4_hl,
    /* 0xE7 */ [SET_4_A]   = set_4_a,
    /* 0xE8 */ [SET_5_B]   = set_5_b,
    /* 0xE9 */ [SET_5_C]   = set_5_c,
    /* 0xEA */ [SET_5_D]   = set_5_d,
    /* 0xEB */ [SET_5_E]   = set_5_e,
    /* 0xEC */ [SET_5_H]   = set_5_h,
    /* 0xED */ [SET_5_L]   = set_5_l,
    /* 0xEE */ [SET_5_HL]  = set_5_hl,
    /* 0xEF */ [SET_5_A]   = set_5_a,
    /*              ROW 15                */
    /* 0xF0 */ [SET_6_B]   = set_6_b,   
    /* 0xF1 */ [SET_6_C]   = set_6_c,
    /* 0xF2 */ [SET_6_D]   = set_6_d,
    /* 0xF3 */ [SET_6_E]   = set_6_e,
    /* 0xF4 */ [SET_6_H]   = set_6_h,
    /* 0xF5 */ [SET_6_L]   = set_6_l,
    /* 0xF6 */ [SET_6_HL]  = set_6_hl,
    /* 0xF7 */ [SET_6_A]   = set_6_a,
    /* 0xF8 */ [SET_7_B]   = set_7_b,
    /* 0xF9 */ [SET_7_C]   = set_7_c,
    /* 0xFA */ [SET_7_D]   = set_7_d,
    /* 0xFB */ [SET_7_E]   = set_7_e,
    /* 0xFC */ [SET_7_H]   = set_7_h,
    /* 0xFD */ [SET_7_L]   = set_7_l,
    /* 0xFE */ [SET_7_HL]  = set_7_hl,
    /* 0xFF */ [SET_7_A]   = set_7_a,
    /*           END OPCODES               */
};

static void execute_opcode(uint8_t opcode)
{
    if(cpu->interrupt_enabled == IE_NEXT_INS) cpu->interrupt_enabled = true;
    if(cpu->testing) printf(" | %s\n", opcode_word[opcode]);
    if (opcode == CB_PREFIX)
    {
        prefix_opcode_table[opcode]();
    }
    else
    {
        opcode_table[opcode]();
    }
}

/* CLIENT (PUBLIC) FUNCTIONS */

bool init_cpu(bool testing)
{
    // init pointers
    cpu    = (CPU*)      malloc(sizeof(CPU));
    memory = (uint8_t*)  malloc(BUS_MEMORY_SIZE);
    R      = (Register*) malloc(sizeof(Register));
    cpu->ins_length = 0;
    // init registers
    R->A = R->F = DEFAULT_8BIT_REG_VAL;
    R->B = R->C = DEFAULT_8BIT_REG_VAL;
    R->D = R->E = DEFAULT_8BIT_REG_VAL;
    R->H = R->L = DEFAULT_8BIT_REG_VAL;  
    cpu->interrupt_enabled = true;
    cpu->speed_enabled     = false;
    cpu->cpu_running       = true;
    cpu->testing           = testing; 
    // init SP and PC registers.
    R->PC = get_rom_start();
    R->SP = HIGH_RAM_ADDRESS_END;
    printf("CPU Initialized...\n\n");
    return true;
}

bool machine_cycle()
{
    uint8_t opcode = fetch();
    printf("%02X", opcode);
    if (opcode == HALT) 
    { 
        printf(" | %s\n", opcode_word[opcode]);
        halt(); 
        return false;
    }
    reset_cycles();
    cpu->ins_length = 0;
    execute_opcode(opcode);
    return cpu->cpu_running;
}

CPU *get_cpu_info()
{
    return cpu;
}

Register *get_register_bank()
{
    return R; 
}

uint8_t *get_cpu_memory()
{
    return memory;
}

void tidy_cpu()
{
    free(cpu);
    cpu = NULL;
    free(memory);
    memory = NULL;
    free(R);
    R = NULL;
}

void reset_pc()
{
    R->PC = get_rom_start();
}

void set_cpu_running(bool is_running)
{
    cpu->cpu_running = is_running;
}

uint8_t get_ins_length()
{
    return cpu->ins_length;
}