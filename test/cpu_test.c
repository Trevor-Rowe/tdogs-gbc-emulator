#include <CUnit/CUnit.h> 
#include <CUnit/Basic.h>
#include "cpu.h"
#include "emulator.h"
#include "clock.h"
#include "common.h"

#define BASE_ROM_DIR = "../roms"
#define FIRST_ROM    = "../roms/first_rom.gb"

static void await_user()
{
    printf("ENTER NUMBER TO CONTINUE...");
    int cont;
    scanf("%d", &cont);
}

static bool is_flag_set(Register *reg_bank, Flag flag)
{
    uint8_t F = reg_bank->F;
    return ((F & flag) == flag);
}

void test_reg_ld_16_nn()
{ // 0x01 - 0x31
    char *file_path = "../roms/test_reg_ld_16_nn.gb";
    init_emulator(file_path, true);
    Register *R = get_register_bank();

    // 0x01 -> LD BC, NN (3, 12) (- - - -)
    start_emulator();
    CU_ASSERT((R->B == MAX_INT_8) && R->C == MAX_INT_8);
    CU_ASSERT(get_cycles() == 12);
    CU_ASSERT(get_ins_length() == 3);

    // 0x11 -> LD DE, NN (3, 12) (- - - -)
    start_emulator();
    CU_ASSERT((R->D == MAX_INT_8) && R->E == MAX_INT_8);
    CU_ASSERT(get_cycles() == 12);
    CU_ASSERT(get_ins_length() == 3);

    // 0x21 -> LD HL, NN (3, 12) (- - - -)
    start_emulator();
    CU_ASSERT((R->H == MAX_INT_8) && R->L == MAX_INT_8); 
    CU_ASSERT(get_cycles() == 12);
    CU_ASSERT(get_ins_length() == 3);

    // 0x31 -> LD SP, NN (3, 12) (- - - -)
    start_emulator(); // Attempts to load 0xFFF - BAD!
    CU_ASSERT((R->SP == HIGH_RAM_ADDRESS_END));
    CU_ASSERT(get_cycles() == 12);
    CU_ASSERT(get_ins_length() == 3); 
    /* more tests...*/
    tidy_emulator();
}

void test_reg_inc_16()
{
    char *file_path = "../roms/test_reg_inc_16.gb";
    init_emulator(file_path, true);
    Register *R = get_register_bank();

    // 0x01 INC BC (1, 8) (- - - -)
    start_emulator();
    CU_ASSERT((R->B == BYTE_OVERFLOW) && (R->C == BYTE_OVERFLOW));
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(get_cycles() == 8);

    // 0x11 INC DE (1, 8) (- - - -)
    start_emulator();
    CU_ASSERT((R->D == BYTE_OVERFLOW) && (R->E == BYTE_OVERFLOW));
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(get_cycles() == 8);

    // 0x21 INC HL (1, 8) (- - - -)
    start_emulator();
    CU_ASSERT((R->H == BYTE_OVERFLOW) && (R->L == BYTE_OVERFLOW));
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(get_cycles() == 8);

    // 0x31 INC SP (1, 8) (- - - -)
    R->SP = HIGH_RAM_ADDRESS_START;
    start_emulator();
    CU_ASSERT(R->SP = HIGH_RAM_ADDRESS_START + 1);
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(get_cycles() == 8);
    
    // Print and Clean Up.
    start_emulator(); // Prints
    tidy_emulator(); 
}

void test_reg_dec_16()
{
    char *file_path = "../roms/test_reg_dec_16.gb";
    init_emulator(file_path, true);
    Register *R = get_register_bank();

    // 0x0B DEC BC (1, 8) (- - - -)
    start_emulator();
    CU_ASSERT((R->B == BYTE_UNDERFLOW) && (R->C == BYTE_UNDERFLOW));
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(get_cycles() == 8);

    // 0x1B DEC DE (1, 8) (- - - -)
    start_emulator();
    CU_ASSERT((R->D == BYTE_UNDERFLOW) && (R->E == BYTE_UNDERFLOW));
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(get_cycles() == 8);

    // 0x2B DEC HL (1, 8) (- - - -)
    start_emulator();
    CU_ASSERT((R->H == BYTE_UNDERFLOW) && (R->L == BYTE_UNDERFLOW));
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(get_cycles() == 8);

    // 0x3B DEC SP (1, 8) (- - - -)
    R->SP = HIGH_RAM_ADDRESS_END;
    start_emulator();
    CU_ASSERT(R->SP = HIGH_RAM_ADDRESS_END - 1);
    CU_ASSERT(get_ins_length() == 1);
    CU_ASSERT(get_cycles() == 8);
    
    // Print and Clean Up.
    start_emulator(); // Prints
    tidy_emulator(); 
}

void test_write_memory()
{
    char *file_path = "../roms/test_write_memory.gb";
}

void test_reg_ld_8()
{
    char *file_path = "../roms/test_reg_ld_8.gb";
}

void test_reg_ld_8_n()
{
    char *file_path = "../roms/test_ld_8_n.gb";
}

void test_reg_inc_8()
{
    char *file_path = "../roms/test_reg_inc_8.gb";
}

void test_reg_dec_8()
{
    char *file_path = "../roms/test_reg_dec_8.gb";
}

void test_reg_add_16()
{
    char *file_path = "../roms/test_reg_add_16.gb";
}

void test_reg_add_8()
{
    char *file_path = "../rom/test_reg_add_8.gb";

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
    CU_pSuite suite_2 = CU_add_suite("Loading/Writing to CPU Memory", 0, 0);
    CU_pSuite suite_3 = CU_add_suite("8-Bit Register Loading", 0, 0);
    CU_pSuite suite_4 = CU_add_suite("8-Bit Register Inc/Dec", 0, 0);
    CU_pSuite suite_5 = CU_add_suite("Addition Operations", 0, 0);
    if 
    (
        (suite_0 == NULL) ||
        (suite_1 == NULL) ||
        (suite_2 == NULL) ||
        (suite_3 == NULL) ||
        (suite_4 == NULL) ||
        (suite_5 == NULL) 
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
        (CU_add_test(suite_1, "Increment 16-Bit Register", test_reg_dec_16) == NULL)
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