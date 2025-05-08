#include <CUnit/CUnit.h> 
#include <CUnit/Basic.h>
#include <stdlib.h>
#include "util.h"

void test_queue_sorting()
{
    Queue    *queue = init_queue(10);
    GbcPixel *pixel = (GbcPixel*) malloc(sizeof(GbcPixel));
    for (int i = (queue->capacity - 1); i >= 0; i--)
    { // [9, 8, 7, 6, 5, 4, 3, 2, 1, 0]
        pixel->x = i; enqueue(queue, pixel); 
    }
    
    sort_oam_by_xpos(queue);
    CU_ASSERT(queue->items[0]->x == 0 && queue->items[9]->x == 9);
    
    tidy_queue(queue);
    free(pixel); pixel = NULL;
}

void test_queue_consistency()
{
    uint8_t    size = 16;
    Queue    *queue = init_queue(size);
    GbcPixel *pixel = (GbcPixel*) malloc(sizeof(GbcPixel));
    for (int i = 0; i < 100; i++)
    {
        for (int j = 0; j < size; j++)
        { // Fill
            pixel->x = i;
            enqueue(queue, pixel);
        }
        for (int j = 0; j < size; j++)
        { // Empty
            dequeue(queue);
        }
    }
    for (int j = 0; j < size; j++)
    { // Fill
        pixel->x = j;
        enqueue(queue, pixel);
    }
    CU_ASSERT(queue->items[15]->x == 15);
    tidy_queue(queue);
    free(pixel); pixel = NULL;
}

void test_queue_edges()
{
    uint8_t    size = 100;
    Queue    *queue = init_queue(size);
    GbcPixel *pixel = (GbcPixel*) pixel;
    for (int i = 0; i < (size * 2); i++)
    {
        enqueue(queue, pixel); 
    }
    CU_ASSERT(is_full(queue) && (queue->size == size));
    for (int i = 0; i < (size * 2); i++)
    {
        dequeue(queue);
    }
    CU_ASSERT(is_empty(queue) && !queue->size);
    tidy_queue(queue);
    free(pixel); pixel = NULL;
}

int main()
{
    // Initialize the CUnit test registry
    if (CUE_SUCCESS != CU_initialize_registry()) 
    {
        return CU_get_error();
    }

    // Create a test suite
    CU_pSuite suite = CU_add_suite("Circular Queue Tests", 0, 0);
    if (suite == NULL) 
    {
        CU_cleanup_registry();
        return CU_get_error();
    }

    // Add test cases to the suite
    if 
    (
        (CU_add_test(suite, "Sort Queue by X Position", test_queue_sorting) == NULL),
        (CU_add_test(suite, "Check Queue Consistency",  test_queue_consistency) == NULL),
        (CU_add_test(suite, "Queue Edge Cases", test_queue_edges) == NULL)
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