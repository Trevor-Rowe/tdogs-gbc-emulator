#include <CUnit/CUnit.h> 
#include <CUnit/Basic.h>
#include <stdbool.h>
#include "cpu.h"
#include "memory.h"
#include "emulator.h"
#include "common.h"
#include "logger.h"

#define BASE_ROM_DIR = "../roms/testing"
#define FIRST_ROM    = "../roms/testing/first_rom.gb"
#define LOG_MESSAGE(level, format, ...) log_message(level, __FILE__, __func__, format, ##__VA_ARGS__)

void test_reg_ld_16_nn()
{ // 0x01 - 0x31
    char *file_path = "../roms/testing/test_reg_ld_16_nn.gb";
    init_emulator(file_path, true);
    Register *R = get_register_bank();

    // 0x01 -> LD BC, NN (3, 12) (- - - -)
    start_emulator(NULL);
    CU_ASSERT((R->B == MAX_INT_8) && R->C == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 3);

    // 0x11 -> LD DE, NN (3, 12) (- - - -)
    start_emulator(NULL);
    CU_ASSERT((R->D == MAX_INT_8) && R->E == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 3);

    // 0x21 -> LD HL, NN (3, 12) (- - - -)
    start_emulator(NULL);
    CU_ASSERT((R->H == MAX_INT_8) && R->L == MAX_INT_8); 
    CU_ASSERT(get_ins_length() == 3);

    // 0x31 -> LD SP, NN (3, 12) (- - - -)
    start_emulator(NULL); // Attempts to load 0xFFF - BAD!
    CU_ASSERT((R->SP == HIGH_RAM_ADDRESS_END));
    CU_ASSERT(get_ins_length() == 3); 
    /* more tests...*/
    tidy_emulator();
}

void test_reg_inc_16()
{
    char *file_path = "../roms/testing/test_reg_inc_16.gb";
    init_emulator(file_path, true);
    Register *R = get_register_bank();

    // 0x01 INC BC (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT((R->B == BYTE_OVERFLOW) && (R->C == BYTE_OVERFLOW));
    CU_ASSERT(get_ins_length() == 1);

    // 0x11 INC DE (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT((R->D == BYTE_OVERFLOW) && (R->E == BYTE_OVERFLOW));
    CU_ASSERT(get_ins_length() == 1);

    // 0x21 INC HL (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT((R->H == BYTE_OVERFLOW) && (R->L == BYTE_OVERFLOW));
    CU_ASSERT(get_ins_length() == 1);

    // 0x31 INC SP (1, 8) (- - - -)
    R->SP = HIGH_RAM_ADDRESS_START;
    start_emulator(NULL);
    CU_ASSERT(R->SP = HIGH_RAM_ADDRESS_START + 1);
    CU_ASSERT(get_ins_length() == 1);
    
    // Print and Clean Up.
    start_emulator(NULL); // Prints
    tidy_emulator(); 
}

void test_reg_dec_16()
{
    char *file_path = "../roms/testing/test_reg_dec_16.gb";
    init_emulator(file_path, true);
    Register *R = get_register_bank();

    // 0x0B DEC BC (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT((R->B == BYTE_UNDERFLOW) && (R->C == BYTE_UNDERFLOW));
    CU_ASSERT(get_ins_length() == 1);

    // 0x1B DEC DE (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT((R->D == BYTE_UNDERFLOW) && (R->E == BYTE_UNDERFLOW));
    CU_ASSERT(get_ins_length() == 1);

    // 0x2B DEC HL (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT((R->H == BYTE_UNDERFLOW) && (R->L == BYTE_UNDERFLOW));
    CU_ASSERT(get_ins_length() == 1);

    // 0x3B DEC SP (1, 8) (- - - -)
    R->SP = HIGH_RAM_ADDRESS_END;
    start_emulator(NULL);
    CU_ASSERT(R->SP = HIGH_RAM_ADDRESS_END - 1);
    CU_ASSERT(get_ins_length() == 1);
    
    // Print and Clean Up.
    start_emulator(NULL); // Prints
    tidy_emulator(); 
}

void test_stack()
{
    char *file_path = "../roms/testing/test_stack.gb";
    init_emulator(file_path, true);
    Register *R = get_register_bank();

    // 0xC5 PUSH BC (1, 16) (- - - -)
    R->B = MAX_INT_8; R->C = MAX_INT_8; 
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1)
    // 0xC1 POP  BC (1, 12) (- - - -)
    R->B = IS_ZERO; R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == MAX_INT_8 && R->C == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1)

    // 0xD5 PUSH DE (1, 16) (- - - -)
    R->D = MAX_INT_8; R->E = MAX_INT_8; 
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1)
    // 0xD1 POP  DE (1, 12) (- - - -)
    R->D = IS_ZERO; R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == MAX_INT_8 && R->E == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1)

    // 0xE5 PUSH HL (1, 16) (- - - -)
    R->H = MAX_INT_8; R->L = MAX_INT_8; 
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1)
    // 0xE1 POP  HL (1, 12) (- - - -)
    R->H = IS_ZERO; R->L = IS_ZERO; 
    start_emulator(NULL);
    CU_ASSERT(R->H == MAX_INT_8 && R->L == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1)

    // 0xF1 POP AF (1)
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8 && R->F == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1)

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_ld_8_n()
{
    char *file_path = "../roms/testing/test_reg_ld_8_n.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0x06 LD B, N (2, 8) (- - - -) | 0xFF
    start_emulator(NULL);
    CU_ASSERT(R->B == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2)
    
    // 0x16 LD D, N (2, 8) (- - - -) | 0xFF
    start_emulator(NULL);
    CU_ASSERT(R->D == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2)
    
    // 0x26 LD H, N (2, 8) (- - - -) | 0x9F
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x9F);
    CU_ASSERT(get_ins_length() == 2)

    // 0x0E LD C, N (2, 8) (- - - -) | 0xFF
    start_emulator(NULL);
    CU_ASSERT(R->C == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2)
    
    // 0x1E LD E, N (2, 8) (- - - -) | 0xFF
    start_emulator(NULL);
    CU_ASSERT(R->E == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2)
    
    // 0x2E LD L, N (2, 8) (- - - -) | 0xFF
    start_emulator(NULL);
    CU_ASSERT(R->L == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2)
    
    // 0x3E LD A, N (2, 8) (- - - -) | 0xFF
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2)

    // 0x36 LD [HL], N (2, 12) (- - - -) [0x9FFF]
    start_emulator(NULL);
    CU_ASSERT(memory[0x9FFF] == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2)

    // 0xEA & 0xFA

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_ld_8_mem()
{
    char *file_path = "../roms/testing/test_reg_ld_8_mem.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();
    
    // 0x02 LD [BC], A (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x12 LD [DE], A (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT(memory[0xA001] == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0x22 LD [HL+], A (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT(memory[0xA002] == MAX_INT_8);
    CU_ASSERT(R->H == 0xA0 && R->L == 0x03);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x32 LD [HL-], A (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT(memory[0xA003] == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x0A LD A, [BC] (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x1A LD A, [DE] (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x2A LD A, [HL+] (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT((R->H == 0xA0) && (R->L == 0x03));
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x3A LD A, [HL-] (1, 8) (- - - -)
    start_emulator(NULL);
    CU_ASSERT((R->H == 0xA0) && (R->L == 0x02));
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);
 
    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_ld_8()
{ // Care on file alignment and addressing memory...
    char *file_path = "../roms/testing/test_reg_ld_8.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();
    // Unit Testing Rev. 

    // 0x40 LD B, B (1, 4)
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);

    // 0x41 LD B, C (1, 4)
    R->B = IS_ZERO; R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->B == R->C);
    CU_ASSERT(get_ins_length() == 1);

    // 0x42 LD B, D (1, 4)
    R->B = IS_ZERO; R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->B == R->D);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x43 LD B, E (1, 4)
    R->B = IS_ZERO; R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->B == R->E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x44 LD B, H (1, 4)
    R->B = IS_ZERO; R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->B == R->H);
    CU_ASSERT(get_ins_length() == 1);

    // 0x45 LD B, L (1, 4)
    R->B = IS_ZERO; R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->B == R->L);
    CU_ASSERT(get_ins_length() == 1);

    // 0x46 LD B, [HL] (1, 8)
    R->H = 0xA0; R->L = 0x00;
    R->B = IS_ZERO; memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->B == memory[0xA000]);
    CU_ASSERT(get_ins_length() == 1);

    // 0x47 LD B, A (1, 4)
    R->B = IS_ZERO; R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->B == R->A);
    CU_ASSERT(get_ins_length() == 1);

    // 0x48 LD C, B (1, 4)
    R->C = IS_ZERO; R->B = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == R->B);
    CU_ASSERT(get_ins_length() == 1);

    // 0x49 LD C, C (1, 4)
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);

    // 0x4A LD C, D (1, 4)
    R->C = IS_ZERO; R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == R->D);
    CU_ASSERT(get_ins_length() == 1);

    // 0x4B LD C, E (1, 4)
    R->C = IS_ZERO; R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == R->E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x4C LD C, H (1, 4)
    R->C = IS_ZERO; R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == R->H);
    CU_ASSERT(get_ins_length() == 1);

    // 0x4D LD C, L (1, 4)
    R->C = IS_ZERO; R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == R->L);
    CU_ASSERT(get_ins_length() == 1);

    // 0x4E LD C, [HL] (1, 8)
    R->H = 0xA0; R->L = 0x00;
    R->C = IS_ZERO; memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == memory[0xA000]);
    CU_ASSERT(get_ins_length() == 1);

    // 0x4F LD C, A (1, 4)
    R->C = IS_ZERO; R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == R->A);
    CU_ASSERT(get_ins_length() == 1);

    // 0x5X

    // 0x50 LD D, B (1, 4)
    R->D = IS_ZERO; R->B = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == R->B);
    CU_ASSERT(get_ins_length() == 1);

    // 0x51 LD D, C (1, 4)
    R->D = IS_ZERO; R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == R->C);
    CU_ASSERT(get_ins_length() == 1);

    // 0x52 LD D, D (1, 4)
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);

    // 0x53 LD D, E (1, 4)
    R->D = IS_ZERO; R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == R->E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x54 LD D, H (1, 4)
    R->D = IS_ZERO; R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == R->H);
    CU_ASSERT(get_ins_length() == 1);

    // 0x55 LD D, L (1, 4)
    R->D = IS_ZERO; R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == R->L);
    CU_ASSERT(get_ins_length() == 1);

    // 0x56 LD D, [HL] (1, 4)
    R->H = 0xA0; R->L = 0x00; 
    R->D = IS_ZERO; memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == memory[0xA000]);
    CU_ASSERT(get_ins_length() == 1);

    // 0x57 LD D, A (1, 4)
    R->D = IS_ZERO; R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == R->A);
    CU_ASSERT(get_ins_length() == 1);

    // 0x58 LD E, B (1, 4)
    R->E = IS_ZERO; R->B = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->E == R->B);
    CU_ASSERT(get_ins_length() == 1);

    // 0x59 LD E, C (1, 4)
    R->E = IS_ZERO; R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->E == R->C);
    CU_ASSERT(get_ins_length() == 1);

    // 0x5A LD E, D (1, 4)
    R->E = IS_ZERO; R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->E == R->D);
    CU_ASSERT(get_ins_length() == 1);

    // 0x5B LD E, E (1, 4)
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);

    // 0x5C LD E, H (1, 4)
    R->E = IS_ZERO; R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->E == R->H);
    CU_ASSERT(get_ins_length() == 1);

    // 0x5D LD E, L (1, 4)
    R->E = IS_ZERO; R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->E == R->L);
    CU_ASSERT(get_ins_length() == 1);

    // 0x5E LD E, [HL] (1, 4)
    R->H = 0xA0; R->L = 0x00;
    R->E = IS_ZERO; memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->E == memory[0xA000]);
    CU_ASSERT(get_ins_length() == 1);

    // 0x5F LD E, A (1, 4)
    R->E = IS_ZERO; R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->E == R->A);
    CU_ASSERT(get_ins_length() == 1);

    // 0x6X

    // 0x60 LD H, B (1, 4)
    R->H = IS_ZERO; R->B = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->H == R->B);
    CU_ASSERT(get_ins_length() == 1);

    // 0x61 LD H, C (1, 4)
    R->H = IS_ZERO; R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->H == R->C);
    CU_ASSERT(get_ins_length() == 1);

    // 0x62 LD H, D (1, 4)
    R->H = IS_ZERO; R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->H == R->D);
    CU_ASSERT(get_ins_length() == 1);

    // 0x63 LD H, E (1, 4)
    R->H = IS_ZERO; R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->H == R->E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x64 LD H, H (1, 4)
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);

    // 0x65 LD H, L (1, 4)
    R->H = IS_ZERO; R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->H == R->L);
    CU_ASSERT(get_ins_length() == 1);

    // 0x66 LD H, [HL] (1, 8)
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->H == memory[0xA000]);
    CU_ASSERT(get_ins_length() == 1);

    // 0x67 LD H, A (1, 4)
    R->H = IS_ZERO; R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->H == R->A);
    CU_ASSERT(get_ins_length() == 1);

    // 0x68 LD L, B (1, 4)
    R->L = IS_ZERO; R->B = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == R->B);
    CU_ASSERT(get_ins_length() == 1);

    // 0x69 LD L, C (1, 4)
    R->L = IS_ZERO; R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == R->C);
    CU_ASSERT(get_ins_length() == 1);

    // 0x6A LD L, D (1, 4)
    R->L = IS_ZERO; R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == R->D);
    CU_ASSERT(get_ins_length() == 1);

    // 0x6B LD L, E (1, 4)
    R->L = IS_ZERO; R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == R->E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x6C LD L, H (1, 4)
    R->L = IS_ZERO; R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == R->H);
    CU_ASSERT(get_ins_length() == 1);

    // 0x6D LD L, L (1, 4)
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);

    // 0x6E LD L, [HL] (1, 8)
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == memory[0xA000]);
    CU_ASSERT(get_ins_length() == 1);

    // 0x6F LD L, A (1, 4)
    R->L = IS_ZERO; R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == R->A);
    CU_ASSERT(get_ins_length() == 1);

    // 0x7X

    // 0x70 LD [HL], B (1, 4)
    R->H = 0xA0; R->L = 0x00; 
    memory[0xA000] = IS_ZERO; R->B = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == R->B);
    CU_ASSERT(get_ins_length() == 1);

    // 0x71 LD [HL], C (1, 4)
    R->H = 0xA0; R->L = 0x00; 
    memory[0xA000] = IS_ZERO; R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == R->C);
    CU_ASSERT(get_ins_length() == 1);

    // 0x72 LD [HL], D (1, 4)
    R->H = 0xA0; R->L = 0x00; 
    memory[0xA000] = IS_ZERO; R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == R->D);
    CU_ASSERT(get_ins_length() == 1);

    // 0x73 LD [HL], E (1, 4)
    R->H = 0xA0; R->L = 0x00; 
    memory[0xA000] = IS_ZERO; R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == R->E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x74 LD [HL], H (1, 4)
    R->H = 0xA0; R->L = 0x00; 
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == R->H);
    CU_ASSERT(get_ins_length() == 1);

    // 0x75 LD [HL], L (1, 4)
    R->H = 0xA0; R->L = 0x00; 
    memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == R->L);
    CU_ASSERT(get_ins_length() == 1);

    // 0x76 HALT
    start_emulator(NULL);

    // 0x77 LD [HL], A (1, 4)
    R->H = 0xA0; R->L = 0x00; 
    memory[0xA000] = IS_ZERO; R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == R->A);
    CU_ASSERT(get_ins_length() == 1);

    // 0x78 LD A, B (1, 4)
    R->A = IS_ZERO; R->B = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == R->B);
    CU_ASSERT(get_ins_length() == 1);

    // 0x79 LD A, C (1, 4)
    R->A = IS_ZERO; R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == R->C);
    CU_ASSERT(get_ins_length() == 1);

    // 0x7A LD A, D (1, 4)
    R->A = IS_ZERO; R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == R->D);
    CU_ASSERT(get_ins_length() == 1);

    // 0x7B LD A, E (1, 4)
    R->A = IS_ZERO; R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == R->E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x7C LD A, H (1, 4)
    R->A = IS_ZERO; R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == R->H);
    CU_ASSERT(get_ins_length() == 1);

    // 0x7D LD A, L (1, 4)
    R->A = IS_ZERO; R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == R->L);
    CU_ASSERT(get_ins_length() == 1);

    // 0x7E LD A, [HL] (1, 4)
    R->H = 0xA0; R->L = 0x00;
    R->A = IS_ZERO; memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == memory[0xA000]);
    CU_ASSERT(get_ins_length() == 1);

    // 0x7F LD A, A (1, 4)
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_inc_8()
{
    char *file_path = "../roms/testing/test_reg_inc_8.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0x04 INC B (1, 4) (Z 0 H -)
    R->B = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x10);
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x14 INC D (1, 4) (Z 0 H -)
    R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == IS_ZERO);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x24 INC H (1, 4) (Z 0 H -)
    R->H = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x01);
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x34 INC [HL] (1, 12) (Z 0 H -)
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x01);
    CU_ASSERT(get_ins_length() == 1);

    // 0x0C INC C (1, 4) (Z 0 H -)
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C = 0x01);
    CU_ASSERT(get_ins_length() == 1);

    // 0x1C INC E (1, 4) (Z 0 H -)
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E = 0x01);
    CU_ASSERT(get_ins_length() == 1);

    // 0x2C INC L (1, 4) (Z 0 H -)
    R->L = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->L = 0x01);
    CU_ASSERT(get_ins_length() == 1);

    // 0x3C INC A (1, 4) (Z 0 H -)
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A = 0x01);
    CU_ASSERT(get_ins_length() == 1);

    start_emulator(NULL);
    tidy_emulator();

}

void test_reg_dec_8()
{ // (Z 1 H -)
    char *file_path = "../roms/testing/test_reg_dec_8.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0x05 DEC B (1, 4)
    R->B = 0x10;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x0F);
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(get_ins_length() == 1);

    // 0x15 DEC D (1, 4)
    R->D = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->D == IS_ZERO);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(get_ins_length() == 1);

    // 0x25 DEC H (1, 4)
    R->H = 0x00;
    start_emulator(NULL);
    CU_ASSERT(R->H == MAX_INT_8);
    CU_ASSERT(is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(get_ins_length() == 1);

    // 0x35 DEC [HL] (1, 12)
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0x0D DEC C (1, 4)
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0x1D DEC E (1, 4)
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0x2D DEC L (1, 4)
    R->L = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->L == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0x3D DEC A (1, 4)
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_add_16()
{ // (1, 8) (- 0 H C)
    char *file_path = "../roms/testing/test_reg_add_16.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0x09 ADD HL, BC
    R->H = 0xFF; R->L = 0xFF;
    R->B = 0x00; R->C = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x00 && R->L == 0x00);
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(is_flag_set(CARRY_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x19 ADD HL, DE
    R->H = 0xFF; R->L = 0xFF;
    R->D = 0x00; R->E = 0x10;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x00 && R->L == 0x0F);
    CU_ASSERT(get_ins_length() == 1);

    // 0x29 ADD HL, HL
    R->H = 0x00; R->L = 0xFF;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x01 && R->L == 0xFE);
    CU_ASSERT(get_ins_length() == 1);

    // 0x39 ADD HL, SP
    R->H = 0x00; R->L = 0x00; R->SP = 0xFFFE;
    start_emulator(NULL);
    CU_ASSERT(R->H = 0xFF && R->L == 0xFE);
    CU_ASSERT(get_ins_length() == 1);

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_add_8()
{ // (1, 4) (Z 0 H C)
    char *file_path = "../roms/testing/test_reg_add_8.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0x80 ADD A, B
    R->A = 0x0F; R->B = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(get_ins_length() == 1);

    // 0x81 ADD A, C
    R->A = 0xFF; R->C = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(is_flag_set(CARRY_FLAG))
    CU_ASSERT(get_ins_length() == 1);

    // 0x82 ADD A, D
    R->A = 0x0F; R->D = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x83 ADD A, E
    R->A = 0x0F; R->E = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x84 ADD A, H
    R->A = 0x0F; R->H = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x85 ADD A, L
    R->A = 0x0F; R->L = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x86 ADD A, [HL] (1, 8)
    R->H = 0xA0; R->L = 0x00;
    R->A = 0x0F; memory[0xA000] = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0x87 ADD A, A
    R->A = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x88 ADC A, B
    R->A = 0x0F; R->B = 0x0F; R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1F);
    CU_ASSERT(get_ins_length() == 1);

    // 0x89 ADC A, C
    R->A = 0x0F; R->C = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x8A ADC A, D
    R->A = 0x0F; R->D = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x8B ADC A, E
    R->A = 0x0F; R->E = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x8C ADC A, H
    R->A = 0x0F; R->H = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x8D ADC A, L
    R->A = 0x0F; R->L = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(get_ins_length() == 1);

    // 0x8E ADC A, [HL] (1, 8)
    R->H = 0xA0; R->L = 0x00; R->F = CARRY_FLAG;
    R->A = 0x0F; memory[0xA000] = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1F);
    CU_ASSERT(get_ins_length() == 1);

    // 0x8F ADC A, A
    R->A = 0x0F; R->B =  R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1F);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0xC6 ADD A, N (2, 8)
    R->A = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1E);
    CU_ASSERT(get_ins_length() == 2);
    
    // 0xCE ADC A, N (2, 8)
    R->A = 0x0F; R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x1F);
    CU_ASSERT(get_ins_length() == 2);

    start_emulator(NULL);
    tidy_emulator();

}

void test_reg_sub_8()
{ // (1, 4) (Z 1 H C)
    char *file_path = "../roms/testing/test_reg_sub_8.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0x90 SUB A, B
    R->A = 0x00; R->B = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(is_flag_set(CARRY_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(get_ins_length()  == 1);

    // 0x91 SUB A, C
    R->A = 0x01; R->C = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(get_ins_length()  == 1);

    // 0x92 SUB A, D
    R->A = 0x02; R->D = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x01);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(get_ins_length()  == 1);
    
    // 0x93 SUB A, E
    R->A = 0x01; R->E = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length()  == 1);

    // 0x94 SUB A, H
    R->A = 0x01; R->H = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length()  == 1);

    // 0x95 SUB A, L
    R->A = 0x01; R->L = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length()  == 1);

    // 0x96 SUB A, [HL] (1, 8)
    R->H = 0xA0; R->L = 0x00;
    R->A = 0x01; memory[0xA000] = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length()  == 1);

    // 0x97 SUB A, A
    R->A = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length()  == 1);

    // 0x98 SBC A, B
    R->A = 0x02; R->B = 0x01; R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length()  == 1);

    // 0x99 SBC A, C
    R->A = 0x01; R->C = 0x01; R->F = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length()  == 1);

    // 0x9A SBC A, D
    R->A = 0x01; R->D = 0x01; R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length()  == 1);

    // 0x9B SBC A, E
    R->A = 0x01; R->E = 0x01; R->F = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length()  == 1);

    // 0x9C SBC A, H
    R->A = 0x01; R->H = 0x01; R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length()  == 1);

    // 0x9D SBC A, L
    R->A = 0x01; R->L = 0x01; R->F = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length()  == 1);

    // 0x9E SBC A, [HL] (1, 8)
    R->H = 0xA0; R->L = 0x00; R->F = CARRY_FLAG;
    R->A = 0x02; memory[0xA000] = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length()  == 1);

    // 0x9F SBC A, A
    R->A = 0x01; R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length()  == 1);

    // 0xD6 SUB A, N (2, 8)
    R->A = 0xFF;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0xF0);
    CU_ASSERT(get_ins_length()  == 2);

    // 0xDE SBC A, N (2, 8)
    R->A = 0xFF; R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0xEF);
    CU_ASSERT(get_ins_length()  == 2);

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_and_8()
{ // (Z 0 1 0)
    char *file_path = "../roms/testing/test_reg_and_8.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0xA0 AND A, B
    R->A = 0xF0; R->B = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length() == 1);

    // 0xA1 AND A, C
    R->A = 0xF0; R->C = 0xF0;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0xF0);
    CU_ASSERT(get_ins_length() == 1);

    // 0xA2 AND A, D
    R->A = 0xF0; R->D = 0xF0;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0xF0);
    CU_ASSERT(get_ins_length() == 1);

    // 0xA3 AND A, E
    R->A = MAX_INT_8; R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xA4 AND A, H
    R->A = MAX_INT_8; R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xA5 AND A, L
    R->A = 0x0F; R->L = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x0F);
    CU_ASSERT(get_ins_length() == 1);

    // 0xA6 AND A, [HL] (1, 8)
    R->H = 0xA0; R->L = 0x00;
    R->A = 0xF0; memory[0xA000] = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0xA7 AND A, A
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xE6 AND A, N (2, 8)
    R->A = 0xF0;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0xF0);
    CU_ASSERT(get_ins_length() == 2);

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_xor_8()
{ // (Z 0 0 0)
    char *file_path = "../roms/testing/test_reg_xor_8.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0xA8 XOR A, B
    R->A = 0xF0; R->B = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xA9 XOR A, C
    R->A = MAX_INT_8; R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length() == 1);

    // 0xAA XOR A, D
    R->A = MAX_INT_8; R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length() == 1);

    // 0xAB XOR A, E
    R->A = 0xF0; R->E = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xAC XOR A, H
    R->A = 0xF0; R->H = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xAD XOR A, L
    R->A = 0xF0; R->L = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xAE XOR A, [HL] (1, 8)
    R->H = 0xA0; R->L = 0x00;
    R->A = 0xF0; memory[0xA000] = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xAF XOR A, A
    R->A = 0xF0;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length() == 1);

    // 0xEE XOR A, N (2, 8)
    R->A = 0xF0;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2);

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_or_8()
{ // (Z 0 0 0)
    char *file_path = "../roms/testing/test_reg_or_8.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0xB0 OR A, B
    R->A = IS_ZERO; R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length() == 1);

    // 0xB1 OR A, C
    R->A = 0x0F; R->C = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A = 0x0F);
    CU_ASSERT(get_ins_length() == 1);

    // 0xB2 OR A, D
    R->A = 0xF0; R->D = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A = MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0xB3 OR A, E
    R->A = 0xF0; R->E = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A = MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xB4 OR A, H
    R->A = 0xF0; R->H = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A = MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xB5 OR A, L
    R->A = 0xF0; R->L = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A = MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xB6 OR A, [HL] (1, 8)
    R->H = 0xA0; R->L = 0x00;
    R->A = 0xF0; memory[0xA000] = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A = MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xB7 OR A, A
    R->A = 0xF0;
    start_emulator(NULL);
    CU_ASSERT(R->A = 0xF0);
    CU_ASSERT(get_ins_length() == 1);

    // 0xF6 OR A, N (2, 8)
    R->A = 0xF0;
    start_emulator(NULL);
    CU_ASSERT(R->A = MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2);

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_cp_8()
{ // (Z 1 H C)
    char *file_path = "../roms/testing/test_reg_cp_8.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // START HER EAND REVIEW CP INS Tests.
    
    // 0xB8 CP A, B
    R->A = MAX_INT_8; R->B = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));
    CU_ASSERT(get_ins_length() == 1);

    // 0xB9 CP A, C
    R->A = 0xFE; R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(is_flag_set(CARRY_FLAG));
    CU_ASSERT(get_ins_length() == 1);

    // 0xBA CP A, D
    R->A = MAX_INT_8; R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));
    CU_ASSERT(get_ins_length() == 1);

    // 0xBB CP A, E
    R->A = 0x10; R->E = 0x01;
    start_emulator(NULL);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));
    CU_ASSERT(get_ins_length() == 1);

    // 0xBC CP A, H
    R->A = MAX_INT_8; R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));
    CU_ASSERT(get_ins_length() == 1);

    // 0xBD CP A, L
    R->A = MAX_INT_8; R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));
    CU_ASSERT(get_ins_length() == 1);

    // 0xBE CP A, [HL] (1, 8)
    R->H = 0xA0; R->L = 0x00;
    R->A = MAX_INT_8; memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));
    CU_ASSERT(get_ins_length() == 1);

    // 0xBF CP A, A
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));
    CU_ASSERT(get_ins_length() == 1);

    // 0xFE CP A, N (2, 8)
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));
    CU_ASSERT(get_ins_length() == 2);

    start_emulator(NULL);
    tidy_emulator();
}

void test_pc_jumps()
{ // (- - - -)
    // 0x20 JR NZ, N (2, 12/8)
    // 0x30 JR NC, N (2, 12/8)
    char *file_path = "../roms/testing/test_jumps.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();


    // 0x18 JR N (2, 12)
    R->PC = 0x003E; // N = 0xF0
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0001);
    CU_ASSERT(get_ins_length() == 2);

    // 0x28 JR Z, N (12/8)
     R->F = ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x05);
    CU_ASSERT(get_ins_length() == 2);
    R->F = (uint8_t) ~ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);

    // 0x38 JR C, N (12/8)
    R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC = 0x0010);
    CU_ASSERT(get_ins_length() == 2);
    R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);

    // 0xC2 JP NZ, NN (3, 16/12)
    R->F = (uint8_t) ~ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0018);
    CU_ASSERT(get_ins_length() == 3);
    R->F = (uint8_t) ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 3);

    // 0xD2 JP NC, NN (3, 16/12)
    R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0021);
    CU_ASSERT(get_ins_length() == 3);
    R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 3);

    // 0xC3 JP NN (3, 16)
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x002A);
    CU_ASSERT(get_ins_length() == 3);

    // 0xE9 JP HL (1, 4)
    R->H = IS_ZERO; R->L = 0x2C;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x002D);
    CU_ASSERT(get_ins_length() == 1);

    // 0xCA JP Z, NN (3, 16/12)
    R->F = ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0031);
    CU_ASSERT(get_ins_length() == 3);
    R->F = (uint8_t) ~ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 3);

    // 0xDA JP C, NN (3, 16/12)
    R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0039);
    CU_ASSERT(get_ins_length() == 3);
    R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 3);
    
    tidy_emulator();
}

void test_returns()
{
    char *file_path = "../roms/testing/test_returns.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0xC0 RET NZ (1, 20/8)
    R->H = 0x00; R->L = 0x05;
    R->F = (uint8_t) ~ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0006);
    CU_ASSERT(get_ins_length() == 1);
    R->F = ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);

    // 0xD0 RET NC (1, 20/8)
    R->H = 0x00; R->L = 0x0D;
    R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x000E);
    CU_ASSERT(get_ins_length() == 1);
    R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);

    // 0xC8 RET Z (1, 20/8)
    R->H = 0x00; R->L = 0x15;
    R->F = ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC = 0x0016);
    CU_ASSERT(get_ins_length() == 1);
    R->F = (uint8_t) ~ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);
    
    // 0xD8 RET C (1, 20/8)
    R->H = 0x00; R->L = 0x1D;
    R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC = 0x001E);
    CU_ASSERT(get_ins_length() == 1);
    R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);
    LOG_MESSAGE(TEST, "Cycles: %d ", get_cycles());

    // 0xC9 RET (1, 16)
    R->H = 0x00; R->L = 0x27;
    start_emulator(NULL);
    CU_ASSERT(R->PC = 0x0028);
    CU_ASSERT(get_ins_length() == 1);

    // 0xD9 RETI (1, 16)
    R->H = 0x00; R->L = 0x2B;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x002C);
    CU_ASSERT(get_ins_length() == 1);

    start_emulator(NULL);
    tidy_emulator(); 
}

void test_rst()
{ // (- - - -) (1, 16)
    char *file_path = "../roms/testing/test_sub_routines.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0xC7 RST $00
    R->PC = 0x010F;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0001);
    CU_ASSERT(memory[R->SP + 1] == 0x01 && memory[R->SP + 2] == 0x10);
    CU_ASSERT(get_ins_length() == 1);

    // 0xD7 RST $10
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0011);
    CU_ASSERT(memory[R->SP + 1] == 0x00 && memory[R->SP + 2] == 0x02);
    LOG_MESSAGE(TEST, "%02X -> %02X", memory[R->SP + 1], R->SP + 1);
    LOG_MESSAGE(TEST, "%02X -> %02X", memory[R->SP + 2], R->SP + 2);
    CU_ASSERT(get_ins_length() == 1);

    // 0xE7 RST $20
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0021);
    CU_ASSERT(memory[R->SP  + 1] == 0x00 && memory[R->SP + 2] == 0x12);
    CU_ASSERT(get_ins_length() == 1);

    // 0xF7 RST $30
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0031);
    CU_ASSERT(memory[R->SP + 1] == 0x00 && memory[R->SP + 2] == 0x22);
    CU_ASSERT(get_ins_length() == 1);

    // 0xCF RST $08
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0009);
    CU_ASSERT(memory[R->SP + 1] == 0x00 && memory[R->SP + 2] == 0x32);
    CU_ASSERT(get_ins_length() == 1);

    // 0xDF RST $18
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0019);
    CU_ASSERT(memory[R->SP + 1] == 0x00 && memory[R->SP + 2] == 0x0A);
    CU_ASSERT(get_ins_length() == 1);

    // 0xEF RST $28
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0029);
    CU_ASSERT(memory[R->SP + 1] == 0x00 && memory[R->SP + 2] == 0x1A);
    CU_ASSERT(get_ins_length() == 1);

    // 0xFF RST $FF
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0039);
    CU_ASSERT(memory[R->SP + 1] == 0x00 && memory[R->SP + 2] == 0x2a);
    CU_ASSERT(get_ins_length() == 1);

    start_emulator(NULL);
    tidy_emulator();
}

void test_calls()
{
    char *file_path = "../roms/testing/test_calls.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0xC4 CALL NZ, NN (3, 24/12)
    R->F = (uint8_t) ~ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0011);
    CU_ASSERT(get_ins_length() == 3);
    R->F = (uint8_t) ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 3);

    // 0xD4 CALL NC, NN (3, 24/12)
    R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0021);
    CU_ASSERT(get_ins_length() == 3);
    R->F = (uint8_t) CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 3);

    // 0xCC CALL Z, NN (3, 24/12)
    R->F = (uint8_t) ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0031);
    CU_ASSERT(get_ins_length() == 3);
    R->F = (uint8_t) ~ZERO_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 3);

    // 0xDC CALL C, NN (3, 24/12)
    R->F = (uint8_t) CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0041);
    CU_ASSERT(get_ins_length() == 3);
    R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 3);

    // 0xCD CALL NN (3, 24)
    start_emulator(NULL);
    CU_ASSERT(R->PC == 0x0051);
    CU_ASSERT(get_ins_length() == 3);

    start_emulator(NULL);
    tidy_emulator();
}

void test_stack_ops()
{
    char *file_path = "../roms/testing/test_stack_ops.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0x08 LD [NN], SP (3, 20)
    memory[0xA000] = 0x02;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA001] == 0xFF && memory[0xA000] == 0xFE);
    CU_ASSERT(get_ins_length() == 3);

    // 0xE8 ADD SP, N (2, 16) (0 0 H C)
    R->SP = 0xA0FF;
    start_emulator(NULL);
    CU_ASSERT(R->SP == 0xA100);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0xF8 LD HL, SP + N (2, 12) (0 0 H C)
    R->SP = 0xFFFE;
    start_emulator(NULL);
    LOG_MESSAGE(TEST, "HL was set to %02X%02X", R->H, R->L);
    CU_ASSERT(R->H == IS_ZERO && R->L == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0xF9 LD SP, HL  (1, 8)
    R->H = 0xFF; R->L = 0xFA;
    start_emulator(NULL);
    CU_ASSERT(R->SP == 0xFFFA);
    CU_ASSERT(get_ins_length() == 1);

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_ldh_8()
{
    char *file_path = "../roms/testing/test_ldh_load.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0xE0 LDH N, A (2, 12)
    R->A = MAX_INT_8;
    start_emulator(NULL);
    LOG_MESSAGE(TEST, "%02X", memory[IO_REGISTERS_START + 0x0F]);
    CU_ASSERT(memory[IO_REGISTERS_START + 0x0F] == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2);

    // 0xF0 LDH A, N (2, 12)
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2);

    // 0xE2 LDH [C], A (1, 8)
    R->C = 0x02; R->A = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(memory[IO_REGISTERS_START + R->A] == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    // 0xF2 LDH A, [C] (1, 8)
    R->A = IS_ZERO; R->C = 0x0F;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);

    start_emulator(NULL);
    tidy_emulator();
}

void test_misc()
{
    char *file_path = "../roms/testing/test_misc.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0x07 RLCA (1, 4) (0 0 0 C)
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x17 RLA (1, 4) (0 0 0 C)
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8 - 1);
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x27 DAA (1, 4) (Z - 0 C)
    R->A = IS_ZERO;
    R->F = HALF_CARRY_FLAG | CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x66);
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(is_flag_set(CARRY_FLAG));
    R->A = 0xFE;
    start_emulator(NULL);
    CU_ASSERT(is_flag_set(CARRY_FLAG));
    R->A = IS_ZERO;
    R->F = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x37 SCF (1, 4) (- 0 0 1)
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x0F RRCA (1, 4) (0 0 0 C)
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x1F RRA (1, 4) (0 0 0 C)
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x7F);
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x2F CPL (1, 4) (- 1 1 -)
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x3F CCF (1, 4) (- 0 0 C)
    R->F = CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    start_emulator(NULL);
    tidy_emulator();
}

/*

    CB Prefixed Opcodes
    Default: (2, 8) | (2, 12-16) for [HL] Instructions

*/ 

void test_reg_rotations()
{ // (Z 0 0 C)

    char *file_path = "../roms/testing/test_rotations.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0x00 RLC B
    R->B = 0x7F;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0xFE);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x01 RLC C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));

    // 0x02 RLC D
    R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x03 RLC E
    R->E = 0x40;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x80);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x04 RLC H
    R->H = 0x20;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x40);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x05 RLC L
    R->L = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->L == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));

    // 0x06 RLC [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x07 RLC A
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));

    // 0x08 RRC B
    R->B = 0xFE;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x7F);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x09 RRC C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));

    // 0x0A RRC D
    R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x0B RRC E
    R->E = 0x80;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x40);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x0C RRC H
    R->H = 0x40;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x20);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x0D RRC L
    R->L = 0x20;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x10);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x0E RRC [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] == MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x0F RRC A
    R->A = 0x10;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x08);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x10 RL B
    R->B = MAX_INT_8; R->F = (uint8_t) CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->B == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x11 RL C
    R->C = IS_ZERO; R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->C == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));

    // 0x12 RL D
    R->D = 0x0F; R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0x1E);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x13 RL E
    R->E = MAX_INT_8; R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0xFE);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x14 RL H
    R->H = 0x01; R->F = (uint8_t) CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x03);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x15 RL L
    R->L = 0x0F; R->F = (uint8_t) CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x1F);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x16 RL [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    R->F = (uint8_t) CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x01);
    CU_ASSERT(get_ins_length() == 2);

    // 0x17 RL A
    R->A = MAX_INT_8; R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x18 RR B
    R->B = 0xFE; R->F = (uint8_t) CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->B == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x19 RR C
    R->C = MAX_INT_8; R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0x7F);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x1A RR D
    R->D = MAX_INT_8; R->F = (uint8_t) CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->D == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x1B RR E
    R->E = MAX_INT_8; R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x7F);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x1C RR H
    R->H = 0x02; R->F = (uint8_t) CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x81);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x1D RR L
    R->L = MAX_INT_8; R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x7F);
    CU_ASSERT(get_ins_length() == 2);

    // 0x1E RR [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = MAX_INT_8;
    R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x7F);
    CU_ASSERT(get_ins_length() == 2);

    // 0x1F RR A
    R->A = MAX_INT_8; R->F = (uint8_t) ~CARRY_FLAG;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x7F);
    CU_ASSERT(get_ins_length() == 2);

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_shifts()
{ // (Z 0 0 0) & (Z 0 0 C)

    char *file_path = "../roms/testing/test_shifts.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0x20 SLA B
    R->B = 0x7F;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x7E);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG)); 

    // 0x21 SLA C
    R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0xFE);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x22 SLA D
    R->D = 0x40;
    start_emulator(NULL);
    CU_ASSERT(R->D == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));

    // 0x23 SLA E
    R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x24 SLA H
    R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x25 SLA L
    R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x26 SLA [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x27 SLA A
    R->A = 0x7F;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x7E);
    CU_ASSERT(get_ins_length() == 2);

    // 0x28 SRA B
    R->B = 0x18;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x0C);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG)); 

    // 0x29 SRA C
    R->C = 0x01;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0x01);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x2A SRA D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));

    // 0x2B SRA E
    R->E = 0x18;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x0C);
    CU_ASSERT(get_ins_length() == 2);

    // 0x2C SRA H
    R->H = 0x18;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x0C);
    CU_ASSERT(get_ins_length() == 2);

    // 0x2D SRA L
    R->L = 0x18;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x0C);
    CU_ASSERT(get_ins_length() == 2);

    // 0x2E SRA [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = 0x18;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x0C);
    CU_ASSERT(get_ins_length() == 2);

    // 0x2F SRA A
    R->A = 0x18;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x0C);
    CU_ASSERT(get_ins_length() == 2);

    // 0x30 SWAP B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG));

    // 0x31 SWAP C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));

    // 0x32 SWAP D
    R->D = 0x81;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0x18);
    CU_ASSERT(get_ins_length() == 2);

    // 0x33 SWAP E
    R->E = 0x81;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x18);
    CU_ASSERT(get_ins_length() == 2);

    // 0x34 SWAP H
    R->H = 0x81;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x18);
    CU_ASSERT(get_ins_length() == 2);

    // 0x35 SWAP L
    R->L = 0x81;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x18);
    CU_ASSERT(get_ins_length() == 2);

    // 0x36 SWAP [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = 0x81;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x18);
    CU_ASSERT(get_ins_length() == 2);

    // 0x37 SWAP A
    R->A = 0x81;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x18);
    CU_ASSERT(get_ins_length() == 2);

    // 0x38 SRL B
    R->B = 0x18;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x0C);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(!is_flag_set(HALF_CARRY_FLAG));
    CU_ASSERT(!is_flag_set(CARRY_FLAG)); 

    // 0x39 SRL C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));

    // 0x3A SRL D
    R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0x7F);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(CARRY_FLAG));

    // 0x3B SRL E
    R->E = 0x10;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    // 0x3C SRL H
    R->H = 0x10;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    // 0x3D SRL L
    R->L = 0x10;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    // 0x3E SRL [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = 0x10;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    // 0x3F SRL A
    R->A = 0x10;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_bit_examine()
{

    char *file_path = "../roms/testing/test_bit_examine.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0x40 BIT 0 B
    R->B = 0x01;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x41 BIT 0 C
    R->C = 0x10;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x42 BIT 0 D
    R->D = 0x01;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x43 BIT 0 E
    R->E = 0x10;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x44 BIT 0 H
    R->H = 0x01;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x45 BIT 0 L
    R->L = 0x10;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x46 BIT 0 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = 0x01;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x47 BIT 0 A
    R->A = 0x10;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x48 BIT 1 B
    R->B = 0x02;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x49 BIT 1 C
    R->C = 0x20;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x4A BIT 1 D
    R->D = 0x02;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x4B BIT 1 E
    R->E = 0x20;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x4C BIT 1 H
    R->H = 0x02;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x4D BIT 1 L
    R->L = 0x20;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x4E BIT 1 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = 0x02;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x4F BIT 1 A
    R->A = 0x20;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x50 BIT 2 B
    R->B = 0x04;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x51 BIT 2 C
    R->C = 0x40;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x52 BIT 2 D
    R->D = 0x04;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x53 BIT 2 E
    R->E = 0x40;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x54 BIT 2 H
    R->H = 0x04;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x55 BIT 2 L
    R->L = 0x40;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x56 BIT 2 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = 0x04;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x57 BIT 2 A
    R->A = 0x40;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x58 BIT 3 B
    R->B = 0x08;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x59 BIT 3 C
    R->C = 0x80;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x5A BIT 3 D
    R->D = 0x08;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x5B BIT 3 E
    R->E = 0x80;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x5C BIT 3 H
    R->H = 0x08;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x5D BIT 3 L
    R->L = 0x80;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x5E BIT 3 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = 0x08;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x5F BIT 3 A
    R->A = 0x80;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x60 BIT 4 B
    R->B = 0x10;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x61 BIT 4 C
    R->C = 0x01;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x62 BIT 4 D
    R->D = 0x10;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x63 BIT 4 E
    R->E = 0x01;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x64 BIT 4 H
    R->H = 0x10;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x65 BIT 4 L
    R->L = 0x01;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x66 BIT 4 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = 0x10;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x67 BIT 4 A
    R->A = 0x01;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x68 BIT 5 B
    R->B = 0x20;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x69 BIT 5 C
    R->C = 0x02;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x6A BIT 5 D
    R->D = 0x20;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x6B BIT 5 E
    R->E = 0x02;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x6C BIT 5 H
    R->H = 0x20;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x6D BIT 5 L
    R->L = 0x02;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x6E BIT 5 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = 0x20;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x6F BIT 5 A
    R->B = 0x02;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x70 BIT 6 B
    R->B = 0x40;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x71 BIT 6 C
    R->C = 0x04;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x72 BIT 6 D
    R->D = 0x40;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x73 BIT 6 E
    R->E = 0x04;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x74 BIT 6 H
    R->H = 0x40;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x75 BIT 6 L
    R->L = 0x04;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x76 BIT 6 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = 0x40;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x77 BIT 6 A
    R->A = 0x04;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x78 BIT 7 B
    R->B = 0x80;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x79 BIT 7 C
    R->C = 0x08;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x7A BIT 7 D
    R->D = 0x80;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x7B BIT 7 E
    R->E = 0x08;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x7C BIT 7 H
    R->H = 0x80;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x7D BIT 7 L
    R->L = 0x08;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x7E BIT 7 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = 0x80;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(!is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    // 0x7F BIT 7 A
    R->A = 0x08;
    start_emulator(NULL);
    CU_ASSERT(get_ins_length() == 2);
    CU_ASSERT(is_flag_set(ZERO_FLAG));
    CU_ASSERT(!is_flag_set(SUBTRACT_FLAG));
    CU_ASSERT(is_flag_set(HALF_CARRY_FLAG));

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_bit_reset()
{

    char *file_path = "../roms/testing/test_bit_res.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0x80 RES 0 B
    R->B = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x81 RES 0 C
    R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x82 RES 0 D
    R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x83 RES 0 E
    R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x84 RES 0 H
    R->H = 0xFE;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x85 RES 0 L
    R->L = 0xFE;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x86 RES 0 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x87 RES 0 A
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0xFE);
    CU_ASSERT(get_ins_length() == 2);

    // 0x88 RES 1 B
    R->B = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0xFD);
    CU_ASSERT(get_ins_length() == 2);

    // 0x89 RES 1 C
    R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0xFD);
    CU_ASSERT(get_ins_length() == 2);

    // 0x8A RES 1 D
    R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0xFD);
    CU_ASSERT(get_ins_length() == 2);

    // 0x8B RES 1 E
    R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0xFD);
    CU_ASSERT(get_ins_length() == 2);

    // 0x8C RES 1 H
    R->H = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->H == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0x8D RES 1 L
    R->L = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->L == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0x8E RES 1 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0x8F RES 1 A
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0x90 RES 2 B
    R->B = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0xFB);
    CU_ASSERT(get_ins_length() == 2);

    // 0x91 RES 2 C
    R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0xFB);
    CU_ASSERT(get_ins_length() == 2);

    // 0x92 RES 2 D
    R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0xFB);
    CU_ASSERT(get_ins_length() == 2);

    // 0x93 RES 2 E
    R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0xFB);
    CU_ASSERT(get_ins_length() == 2);

    // 0x94 RES 2 H
    R->H = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->H == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0x95 RES 2 L
    R->L = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->L == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0x96 RES 2 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0x97 RES 2 A
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0x98 RES 3 B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0x99 RES 3 C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0x9A RES 3 D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0x9B RES 3 E
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0x9C RES 3 H
    R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0xF7);
    CU_ASSERT(get_ins_length() == 2);

    // 0x9D RES 3 L
    R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0xF7);
    CU_ASSERT(get_ins_length() == 2);

    // 0x9E RES 3 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0xF7);
    CU_ASSERT(get_ins_length() == 2);

    // 0x9F RES 3 A
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0xF7);
    CU_ASSERT(get_ins_length() == 2);

    // 0xA0 RES 4 B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xA1 RES 4 C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xA2 RES 4 D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xA3 RES 4 E
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xA4 RES 4 H
    R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0xEF);
    CU_ASSERT(get_ins_length() == 2);

    // 0xA5 RES 4 L
    R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0xEF);
    CU_ASSERT(get_ins_length() == 2);

    // 0xA6 RES 4 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0xEF);
    CU_ASSERT(get_ins_length() == 2);

    // 0xA7 RES 4 A
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0xEF);
    CU_ASSERT(get_ins_length() == 2);

    // 0xA8 RES 5 B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xA9 RES 5 C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xAA RES 5 D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xAB RES 5 E
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xAC RES 5 H
    R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0xDF);
    CU_ASSERT(get_ins_length() == 2);

    // 0xAD RES 5 L
    R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0xDF);
    CU_ASSERT(get_ins_length() == 2);

    // 0xAE RES 5 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xAF RES 5 A
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0xDF);
    CU_ASSERT(get_ins_length() == 2);

    // 0xB0 RES 6 B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xB1 RES 6 C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xB2 RES 6 D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xB3 RES 6 E
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == IS_ZERO);
    CU_ASSERT(get_ins_length() == 2);

    // 0xB4 RES 6 H
    R->H = MAX_INT_8;
    start_emulator(NULL);
    //LOG_MESSAGE(TEST, "H = %02X", R->H);
    CU_ASSERT(R->H == 0xBF);
    CU_ASSERT(get_ins_length() == 2);

    // 0xB5 RES 6 L
    R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0xBF);
    CU_ASSERT(get_ins_length() == 2);

    // 0xB6 RES 6 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0xBF);
    CU_ASSERT(get_ins_length() == 2);

    // 0xB7 RES 6 A
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0xBF);
    CU_ASSERT(get_ins_length() == 2);
    
    // 0xB8 RES 7 B
    R->B = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x7F);
    CU_ASSERT(get_ins_length() == 2);

    // 0xB9 RES 7 C
    R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0x7F);
    CU_ASSERT(get_ins_length() == 2);

    // 0xBA RES 7 D
    R->D = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0x7F);
    CU_ASSERT(get_ins_length() == 2);

    // 0xBB RES 7 E
    R->E = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x7F);
    CU_ASSERT(get_ins_length() == 2);

    // 0xBC RES 7 H
    R->H = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x7F);
    CU_ASSERT(get_ins_length() == 2);

    // 0xBD RES 7 L
    R->L = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x7F);
    CU_ASSERT(get_ins_length() == 2);

    // 0xBE RES 7 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x7F);
    CU_ASSERT(get_ins_length() == 2);

    // 0xBF RES 7 A
    R->A = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x7F);
    CU_ASSERT(get_ins_length() == 2);

    start_emulator(NULL);
    tidy_emulator();
}

void test_reg_bit_set()
{

    char *file_path = "../roms/testing/test_bit_set.gb";
    init_emulator(file_path, true);
    uint8_t *memory = get_memory();
    Register *R = get_register_bank();

    // 0xC0 SET 0 B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x01);
    CU_ASSERT(get_ins_length() == 2);

    // 0xC1 SET 0 C
    R->C = MAX_INT_8;
    start_emulator(NULL);
    CU_ASSERT(R->C == MAX_INT_8);
    CU_ASSERT(get_ins_length() == 2);

    // 0xC2 SET 0 D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0x01);
    CU_ASSERT(get_ins_length() == 2);

    // 0xC3 SET 0 E
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x01);
    CU_ASSERT(get_ins_length() == 2);

    // 0xC4 SET 0 H
    R->H = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x01);
    CU_ASSERT(get_ins_length() == 2);

    // 0xC5 SET 0 L
    R->L = IS_ZERO;
    start_emulator(NULL);
    LOG_MESSAGE(TEST, "L = %02X", R->L);
    CU_ASSERT(R->L == 0x01);
    CU_ASSERT(get_ins_length() == 2);

    // 0xC6 SET 0 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x01);
    CU_ASSERT(get_ins_length() == 2);

    // 0xC7 SET 0 A
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x01);
    CU_ASSERT(get_ins_length() == 2);

    // 0xC8 SET 1 B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x02);
    CU_ASSERT(get_ins_length() == 2);

    // 0xC9 SET 1 C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0x02);
    CU_ASSERT(get_ins_length() == 2);

    // 0xCA SET 1 D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0x02);
    CU_ASSERT(get_ins_length() == 2);

    // 0xCB SET 1 E
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x02);
    CU_ASSERT(get_ins_length() == 2);

    // 0xCC SET 1 H
    R->H = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x02);
    CU_ASSERT(get_ins_length() == 2);

    // 0xCD SET 1 L
    R->L = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x02);
    CU_ASSERT(get_ins_length() == 2);

    // 0xCE SET 1 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x02);
    CU_ASSERT(get_ins_length() == 2);

    // 0xCF SET 1 A
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x02);
    CU_ASSERT(get_ins_length() == 2);

    // 0xD0 SET 2 B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x04);
    CU_ASSERT(get_ins_length() == 2);

    // 0xD1 SET 2 C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0x04);
    CU_ASSERT(get_ins_length() == 2);

    // 0xD2 SET 2 D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0x04);
    CU_ASSERT(get_ins_length() == 2);

    // 0xD3 SET 2 E
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x04);
    CU_ASSERT(get_ins_length() == 2);

    // 0xD4 SET 2 H
    R->H = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x04);
    CU_ASSERT(get_ins_length() == 2);

    // 0xD5 SET 2 L
    R->L = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x04);
    CU_ASSERT(get_ins_length() == 2);

    // 0xD6 SET 2 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x04);
    CU_ASSERT(get_ins_length() == 2);

    // 0xD7 SET 2 A
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x04);
    CU_ASSERT(get_ins_length() == 2);

    // 0xD8 SET 3 B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    // 0xD9 SET 3 C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    // 0xDA SET 3 D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    // 0xDB SET 3 E
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    // 0xDC SET 3 H
    R->H = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    // 0xDD SET 3 L
    R->L = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    // 0xDE SET 3 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    // 0xDF SET 3 A
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x08);
    CU_ASSERT(get_ins_length() == 2);

    // 0xE0 SET 4 B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x10);
    CU_ASSERT(get_ins_length() == 2);

    // 0xE1 SET 4 C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0x10);
    CU_ASSERT(get_ins_length() == 2);

    // 0xE2 SET 4 D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0x10);
    CU_ASSERT(get_ins_length() == 2);

    // 0xE3 SET 4 E
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x10);
    CU_ASSERT(get_ins_length() == 2);

    // 0xE4 SET 4 H
    R->H = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x10);
    CU_ASSERT(get_ins_length() == 2);

    // 0xE5 SET 4 L
    R->L = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x10);
    CU_ASSERT(get_ins_length() == 2);

    // 0xE6 SET 4 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x10);
    CU_ASSERT(get_ins_length() == 2);

    // 0xE7 SET 4 A
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x10);
    CU_ASSERT(get_ins_length() == 2);

    // 0xE8 SET 5 B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x20);
    CU_ASSERT(get_ins_length() == 2);

    // 0xE9 SET 5 C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0x20);
    CU_ASSERT(get_ins_length() == 2);

    // 0xEA SET 5 D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0x20);
    CU_ASSERT(get_ins_length() == 2);

    // 0xEB SET 5 E
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x20);
    CU_ASSERT(get_ins_length() == 2);

    // 0xEC SET 5 H
    R->H = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x20);
    CU_ASSERT(get_ins_length() == 2);

    // 0xED SET 5 L
    R->L = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x20);
    CU_ASSERT(get_ins_length() == 2);

    // 0xEE SET 5 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x20);
    CU_ASSERT(get_ins_length() == 2);

    // 0xEF SET 5 A
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x20);
    CU_ASSERT(get_ins_length() == 2);

    // 0xF0 SET 6 B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x40);
    CU_ASSERT(get_ins_length() == 2);

    // 0xF1 SET 6 C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0x40);
    CU_ASSERT(get_ins_length() == 2);

    // 0xF2 SET 6 D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0x40);
    CU_ASSERT(get_ins_length() == 2);

    // 0xF3 SET 6 E
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x40);
    CU_ASSERT(get_ins_length() == 2);

    // 0xF4 SET 6 H
    R->H = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x40);
    CU_ASSERT(get_ins_length() == 2);

    // 0xF5 SET 6 L
    R->L = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x40);
    CU_ASSERT(get_ins_length() == 2);

    // 0xF6 SET 6 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x40);
    CU_ASSERT(get_ins_length() == 2);

    // 0xF7 SET 6 A
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x40);
    CU_ASSERT(get_ins_length() == 2);

    // 0xF8 SET 7 B
    R->B = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->B == 0x80);
    CU_ASSERT(get_ins_length() == 2);

    // 0xF9 SET 7 C
    R->C = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->C == 0x80);
    CU_ASSERT(get_ins_length() == 2);

    // 0xFA SET 7 D
    R->D = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->D == 0x80);
    CU_ASSERT(get_ins_length() == 2);

    // 0xFB SET 7 E
    R->E = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->E == 0x80);
    CU_ASSERT(get_ins_length() == 2);

    // 0xFC SET 7 H
    R->H = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->H == 0x80);
    CU_ASSERT(get_ins_length() == 2);

    // 0xFD SET 7 L
    R->L = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->L == 0x80);
    CU_ASSERT(get_ins_length() == 2);

    // 0xFE SET 7 [HL]
    R->H = 0xA0; R->L = 0x00;
    memory[0xA000] = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(memory[0xA000] == 0x80);
    CU_ASSERT(get_ins_length() == 2);

    // 0xFF SET 7 A
    R->A = IS_ZERO;
    start_emulator(NULL);
    CU_ASSERT(R->A == 0x80);
    CU_ASSERT(get_ins_length() == 2);

    start_emulator(NULL);
    tidy_emulator();
}

int main()
{
    // Initialize the CUnit test registry
    if (CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    // Create a test suite
    CU_pSuite suite_0 = CU_add_suite("16-Bit Register Loading", 0, 0);
    CU_pSuite suite_1 = CU_add_suite("16-Bit Register Inc/Dec", 0, 0);
    CU_pSuite suite_2 = CU_add_suite("Stack Operations", 0, 0);
    CU_pSuite suite_3 = CU_add_suite("8-Bit Register Loading", 0, 0);
    CU_pSuite suite_4 = CU_add_suite("8-Bit Register Inc/Dec", 0, 0);
    CU_pSuite suite_5 = CU_add_suite("Addition Operations", 0, 0);
    CU_pSuite suite_6 = CU_add_suite("Subtraction Operations", 0, 0); // Start here
    CU_pSuite suite_7 = CU_add_suite("Logical Operations", 0, 0);
    CU_pSuite suite_8 = CU_add_suite("Jump Operations", 0, 0);
    CU_pSuite suite_9 = CU_add_suite("Returns Operations", 0, 0);
    CU_pSuite suite_10 = CU_add_suite("Sub-Routine/Call Operations", 0, 0);
    CU_pSuite suite_11 = CU_add_suite("MISC Operations", 0, 0);
    CU_pSuite suite_12 = CU_add_suite("Rotation Operations", 0, 0);
    CU_pSuite suite_13 = CU_add_suite("Shift Operations", 0, 0);
    CU_pSuite suite_14 = CU_add_suite("Examine Bit Operations", 0, 0);
    CU_pSuite suite_15 = CU_add_suite("Reset Bit Operations", 0, 0);
    CU_pSuite suite_16 = CU_add_suite("Set Bit Operations", 0, 0);
    if 
    (
        (suite_0 == NULL) ||
        (suite_1 == NULL) ||
        (suite_2 == NULL) ||
        (suite_3 == NULL) ||
        (suite_4 == NULL) ||
        (suite_5 == NULL) ||
        (suite_6 == NULL) ||
        (suite_7 == NULL) ||
        (suite_8 == NULL) ||
        (suite_9 == NULL) ||
        (suite_10 == NULL) ||
        (suite_11 == NULL) ||
        (suite_12 == NULL) ||
        (suite_13 == NULL) ||
        (suite_14 == NULL) ||
        (suite_15 == NULL) ||
        (suite_16 == NULL)
    ) 
    {
        CU_cleanup_registry();
        return CU_get_error();
    }

    // Add test cases to the suite
    if 
    (
        (CU_add_test(suite_0, "Load NN Immediate", test_reg_ld_16_nn) == NULL) ||
        (CU_add_test(suite_1, "Increment 16-Bit Register", test_reg_inc_16) == NULL) ||
        (CU_add_test(suite_1, "Increment 16-Bit Register", test_reg_dec_16) == NULL) ||
        (CU_add_test(suite_2, "Push/Pop Stack", test_stack) == NULL) ||
        (CU_add_test(suite_2, "Stack Operations", test_stack_ops) == NULL) ||
        (CU_add_test(suite_3, "8-Bit Register Loading", test_reg_ld_8_n) == NULL) ||
        (CU_add_test(suite_3, "8-Bit Register Loading", test_reg_ld_8_mem) == NULL) ||
        (CU_add_test(suite_3, "8-Bit Register Loading", test_reg_ld_8) == NULL) ||
        (CU_add_test(suite_3, "8-Bit I/O Register Loading", test_reg_ldh_8) == NULL) ||
        (CU_add_test(suite_4, "8-Bit Register Increment", test_reg_inc_8) == NULL) ||
        (CU_add_test(suite_4, "8-Bit Register Decrement", test_reg_dec_8) == NULL) ||
        (CU_add_test(suite_5, "16-Bit Addition", test_reg_add_16) == NULL) ||
        (CU_add_test(suite_5, "8-Bit Addition", test_reg_add_8) == NULL) ||
        (CU_add_test(suite_6, "8-Bit Subtraction", test_reg_sub_8) == NULL) ||
        (CU_add_test(suite_7, "8-Bit AND", test_reg_and_8) == NULL) ||
        (CU_add_test(suite_7, "8-Bit XOR", test_reg_xor_8) == NULL) ||
        (CU_add_test(suite_7, "8-Bit OR", test_reg_or_8) == NULL) ||
        (CU_add_test(suite_7, "8-Bit OR", test_reg_cp_8) == NULL) ||
        (CU_add_test(suite_8, "Jumps", test_pc_jumps) == NULL) ||
        (CU_add_test(suite_9, "Returns", test_returns) == NULL) ||
        (CU_add_test(suite_10, "Sub Routines", test_rst) == NULL) ||
        (CU_add_test(suite_10, "Calls", test_calls) == NULL) ||
        (CU_add_test(suite_11, "MISC", test_misc) == NULL) ||
        (CU_add_test(suite_12, "Rotations", test_reg_rotations) == NULL) ||
        (CU_add_test(suite_13, "Shifts", test_reg_shifts) == NULL) ||
        (CU_add_test(suite_14, "Bit Examines", test_reg_bit_examine) == NULL) ||
        (CU_add_test(suite_15, "Bit Resets", test_reg_bit_reset) == NULL) ||
        (CU_add_test(suite_16, "Bit Sets", test_reg_bit_set) == NULL)
    ) 
    {
        CU_cleanup_registry();
        return CU_get_error();
    }

    // Run all tests using the basic interface
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();

    // Clean up registry
    CU_cleanup_registry();
    return CU_get_error();
}