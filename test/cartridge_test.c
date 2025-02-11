#include <CUnit/CUnit.h> 
#include <CUnit/Basic.h>
#include "cartridge.c"

void test_get_cartridge()
{
    int cart = 1;
    CU_ASSERT_PTR_NULL(cart);
}

int main()
{
    char *file_path = "../roms/Tetris.gb";
    init_cartridge(file_path, false);
    // Initialize the CUnit test registry
    if (CUE_SUCCESS != CU_initialize_registry()) {
        return CU_get_error();
    }

    // Create a test suite
    CU_pSuite suite = CU_add_suite("Cartridge Tests", 0, 0);
    if (suite == NULL) {
        CU_cleanup_registry();
        return CU_get_error();
    }

    // Add test cases to the suite
    if ((CU_add_test(suite, "Cartrdige Copy Test", test_get_cartridge) == NULL)) {
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