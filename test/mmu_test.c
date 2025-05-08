#include <CUnit/CUnit.h> 
#include <CUnit/Basic.h>
#include <stdlib.h>
#include "common.h"
#include "cart.h"
#include "mmu.h"

// gcc -o mmu_test mmu_test.c ../src/mmu.c ../src/cart.c ../src/logger.c ../src/cpu.c -lcunit -I "../include"

void vram_checksum_validation()
{
    init_memory(0xFFFF);
    uint16_t start = VRAM_ADDRESS_START;
    uint16_t   end = VRAM_ADDRESS_END;
    uint8_t scaler = 2;
    long expected = ((end - start) + 1) * scaler;

    write_memory(VBK, 0);
    uint16_t index = start;
    while (index <= end)
    { // Set up checksum.
        write_memory(index, scaler);
        index += 1;
    }

    index = start;
    long checksum = 0;
    while (index <= end)
    { // Calculate checksum.
        checksum += read_memory(index);
        index += 1;
    }

    printf("\n\nVRAM Bank 0 Checksum: %ld == %ld ?", checksum, expected);
    CU_ASSERT(checksum == expected);

    write_memory(VBK, 1);
    index = start;
    while (index <= end)
    { // Set up checksum.
        write_memory(index, scaler);
        index += 1;
    }

    index = start;
    checksum = 0;
    while (index <= end)
    { // Calculate checksum.
        checksum += read_memory(index);
        index += 1;
    }

    printf("\nVRAM Bank 0 Checksum: %ld == %ld ?\n\n", checksum, expected);
    CU_ASSERT(checksum == expected);
    tidy_memory();
}

void wram_checksum_validation()
{
    init_memory(0xFFFF);

    uint8_t scaler = 2;

    // Static Bank 0
    uint16_t  start = WRAM_ZERO_ADDRESS_START;
    uint16_t    end = WRAM_ZERO_ADDRESS_END;
    long   expected = ((end - start) + 1) * scaler;

    write_memory(SVBK, 0);
    uint16_t index = start;
    while (index <= end)
    { // Set up checksum.
        write_memory(index, scaler);
        index += 1;
    }
    index = start;
    long checksum = 0;
    while (index <= end)
    { // Calculate checksum.
        checksum += read_memory(index);
        index += 1;
    }

    CU_ASSERT(checksum == expected);

    // Switchable Banks 1 - 7
       start = WRAM_N_ADDRESS_START;
         end = WRAM_N_ADDRESS_END;
    expected = ((end - start) + 1) * scaler;
    for (int svbk = 1; svbk < 7; svbk++)
    {   
        write_memory(SVBK, svbk);
        index = start;
        while (index <= end)
        { // Set up checksum.
            write_memory(index, scaler);
            index += 1;
        }
        index = start;
        checksum = 0;
        while (index <= end)
        { // Calculate checksum.
            checksum += read_memory(index);
            index += 1;
        }

        CU_ASSERT(checksum == expected);
    }

    tidy_memory();
}

void cram_checksum_validaiton()
{
    init_memory(0xFFFF);

    tidy_memory();
}

int main()
{
    init_cartridge("../roms/gb-test-roms-master/interrupt_time/interrupt_time.gb", 0x0000);
    //init_cartridge("../roms/assembly/graphics_test.gb", 0x0000);
    // Initialize the CUnit test registry
    if (CUE_SUCCESS != CU_initialize_registry()) 
    {
        return CU_get_error();
    }

    // Create a test suite
    CU_pSuite suite = CU_add_suite("MMU Tests", 0, 0);
    if (suite == NULL) 
    {
        CU_cleanup_registry();
        return CU_get_error();
    }

    // Add test cases to the suite
    if 
    (
        CU_add_test(suite, "VRAM Checksum Validation", vram_checksum_validation) == NULL ||
        CU_add_test(suite, "WRAM Checksum Validation", wram_checksum_validation) == NULL
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
    tidy_cartridge();
    return CU_get_error();
}