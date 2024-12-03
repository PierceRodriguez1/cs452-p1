#include <assert.h>
#include <stdlib.h>
#include <time.h>
#ifdef __APPLE__
#include <sys/errno.h>
#else
#include <errno.h>
#endif
#include "harness/unity.h"
#include "../src/lab.h"


void setUp(void) {
  // set stuff up here
}

void tearDown(void) {
  // clean stuff up here
}



/**
 * Check the pool to ensure it is full.
 */
void check_buddy_pool_full(struct buddy_pool *pool)
{
  //A full pool should have all values 0-(kval-1) as empty
  for (size_t i = 0; i < pool->kval_m; i++)
    {
      assert(pool->avail[i].next == &pool->avail[i]);
      assert(pool->avail[i].prev == &pool->avail[i]);
      assert(pool->avail[i].tag == BLOCK_UNUSED);
      assert(pool->avail[i].kval == i);
    }

  //The avail array at kval should have the base block
  assert(pool->avail[pool->kval_m].next->tag == BLOCK_AVAIL);
  assert(pool->avail[pool->kval_m].next->next == &pool->avail[pool->kval_m]);
  assert(pool->avail[pool->kval_m].prev->prev == &pool->avail[pool->kval_m]);

  //Check to make sure the base address points to the starting pool
  //If this fails either buddy_init is wrong or we have corrupted the
  //buddy_pool struct.
  assert(pool->avail[pool->kval_m].next == pool->base);
}

/**
 * Check the pool to ensure it is empty.
 */
void check_buddy_pool_empty(struct buddy_pool *pool)
{
  //An empty pool should have all values 0-(kval) as empty
  for (size_t i = 0; i <= pool->kval_m; i++)
    {
      assert(pool->avail[i].next == &pool->avail[i]);
      assert(pool->avail[i].prev == &pool->avail[i]);
      assert(pool->avail[i].tag == BLOCK_UNUSED);
      assert(pool->avail[i].kval == i);
    }
}

/**
 * Test allocating 1 byte to make sure we split the blocks all the way down
 * to MIN_K size. Then free the block and ensure we end up with a full
 * memory pool again
 */
void test_buddy_malloc_one_byte(void)
{
  fprintf(stderr, "->Test allocating and freeing 1 byte\n");
  struct buddy_pool pool;
  int kval = MIN_K;
  size_t size = UINT64_C(1) << kval;
  buddy_init(&pool, size);
  void *mem = buddy_malloc(&pool, 1);
  //Make sure correct kval was allocated
  buddy_free(&pool, mem);
  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}

/**
 * Tests the allocation of one massive block that should consume the entire memory
 * pool and makes sure that after the pool is empty we correctly fail subsequent calls.
 */
void test_buddy_malloc_one_large(void)
{
  fprintf(stderr, "->Testing size that will consume entire memory pool\n");
  struct buddy_pool pool;
  size_t bytes = UINT64_C(1) << MIN_K;
  buddy_init(&pool, bytes);

  //Ask for an exact K value to be allocated. This test makes assumptions on
  //the internal details of buddy_init.
  size_t ask = bytes - sizeof(struct avail);
  void *mem = buddy_malloc(&pool, ask);
  assert(mem != NULL);

  //Move the pointer back and make sure we got what we expected
  struct avail *tmp = (struct avail *)mem - 1;
  assert(tmp->kval == MIN_K);
  assert(tmp->tag == BLOCK_RESERVED);
  check_buddy_pool_empty(&pool);

  //Verify that a call on an empty tool fails as expected and errno is set to ENOMEM.
  void *fail = buddy_malloc(&pool, 5);
  assert(fail == NULL);
  assert(errno = ENOMEM);

  //Free the memory and then check to make sure everything is OK
  buddy_free(&pool, mem);
  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}

/**
 * Tests to make sure that the struct buddy_pool is correct and all fields
 * have been properly set kval_m, avail[kval_m], and base pointer after a
 * call to init
 */
void test_buddy_init(void)
{
  fprintf(stderr, "->Testing buddy init\n");
  //Loop through all kval MIN_k-DEFAULT_K and make sure we get the correct amount allocated.
  //We will check all the pointer offsets to ensure the pool is all configured correctly
  for (size_t i = MIN_K; i <= DEFAULT_K; i++)
    {
      size_t size = UINT64_C(1) << i;
      struct buddy_pool pool;
      buddy_init(&pool, size);
      check_buddy_pool_full(&pool);
      buddy_destroy(&pool);
    }
}

// Additional test helpers
static void check_memory_content(void* ptr, size_t size, unsigned char value) {
    unsigned char* bytes = (unsigned char*)ptr;
    for (size_t i = 0; i < size; i++) {
        TEST_ASSERT_EQUAL_UINT8(value, bytes[i]);
    }
}

// Test null/invalid inputs
void test_buddy_null_inputs(void) {
    fprintf(stderr, "->Testing null and invalid inputs\n");
    
    struct buddy_pool pool;
    
    // Test null pool initialization
    buddy_init(NULL, 1024);
    
    // Test null pool malloc
    void* ptr = buddy_malloc(NULL, 64);
    TEST_ASSERT_NULL(ptr);
    
    // Test zero size malloc
    buddy_init(&pool, 1024);
    ptr = buddy_malloc(&pool, 0);
    TEST_ASSERT_NULL(ptr);
    
    // Test null free
    buddy_free(NULL, ptr);
    buddy_free(&pool, NULL);
    
    // Test null realloc
    ptr = buddy_realloc(NULL, NULL, 64);
    TEST_ASSERT_NULL(ptr);
    
    buddy_destroy(&pool);
}

// Test boundary conditions for allocation sizes
void test_buddy_size_boundaries(void) {
    fprintf(stderr, "->Testing size boundary conditions\n");
    
    struct buddy_pool pool;
    buddy_init(&pool, 1 << MIN_K);
    
    // Test minimum allocation size
    void* small = buddy_malloc(&pool, 1);
    TEST_ASSERT_NOT_NULL(small);
    
    // Test allocation slightly below and above power of 2
    void* ptr1 = buddy_malloc(&pool, (1 << 7) - 1);
    void* ptr2 = buddy_malloc(&pool, (1 << 7) + 1);
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    
    buddy_free(&pool, small);
    buddy_free(&pool, ptr1);
    buddy_free(&pool, ptr2);
    buddy_destroy(&pool);
}

// Test realloc functionality thoroughly
void test_buddy_realloc_comprehensive(void) {
    fprintf(stderr, "->Testing realloc comprehensively\n");
    
    struct buddy_pool pool;
    buddy_init(&pool, 1 << 20); // 1MB pool
    
    // Test realloc of NULL should work like malloc
    void* ptr = buddy_realloc(&pool, NULL, 64);
    TEST_ASSERT_NOT_NULL(ptr);
    
    // Test realloc to larger size
    memset(ptr, 0xAA, 64);
    void* larger = buddy_realloc(&pool, ptr, 128);
    TEST_ASSERT_NOT_NULL(larger);
    check_memory_content(larger, 64, 0xAA);
    
    // Test realloc to smaller size
    memset(larger, 0xBB, 128);
    void* smaller = buddy_realloc(&pool, larger, 32);
    TEST_ASSERT_NOT_NULL(smaller);
    check_memory_content(smaller, 32, 0xBB);
    
    // Test realloc to zero should free
    void* zero = buddy_realloc(&pool, smaller, 0);
    TEST_ASSERT_NULL(zero);
    
    buddy_destroy(&pool);
}

// Test multiple allocations and frees
void test_buddy_fragmentation(void) {
    fprintf(stderr, "->Testing memory fragmentation scenarios\n");
    
    struct buddy_pool pool;
    buddy_init(&pool, 1 << 20); // 1MB pool
    
    // Allocate multiple blocks of different sizes
    void* ptrs[10];
    size_t sizes[] = {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};
    
    // Allocate all blocks
    for (int i = 0; i < 10; i++) {
        ptrs[i] = buddy_malloc(&pool, sizes[i]);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }
    
    // Free alternate blocks
    for (int i = 0; i < 10; i += 2) {
        buddy_free(&pool, ptrs[i]);
    }
    
    // Reallocate freed blocks
    for (int i = 0; i < 10; i += 2) {
        ptrs[i] = buddy_malloc(&pool, sizes[i]);
        TEST_ASSERT_NOT_NULL(ptrs[i]);
    }
    
    // Free all blocks
    for (int i = 0; i < 10; i++) {
        buddy_free(&pool, ptrs[i]);
    }
    
    // Verify pool is back to initial state
    check_buddy_pool_full(&pool);
    buddy_destroy(&pool);
}


// Test buddy calculation
void test_buddy_calc_functionality(void) {
    fprintf(stderr, "->Testing buddy calculation functionality\n");
    
    struct buddy_pool pool;
    buddy_init(&pool, 1 << 16); // 64KB pool
    
    // Allocate two blocks of the same size
    void* ptr1 = buddy_malloc(&pool, 1024);
    void* ptr2 = buddy_malloc(&pool, 1024);
    TEST_ASSERT_NOT_NULL(ptr1);
    TEST_ASSERT_NOT_NULL(ptr2);
    
    // Get the headers
    struct avail* block1 = (struct avail*)((char*)ptr1 - sizeof(struct avail));
    struct avail* block2 = (struct avail*)((char*)ptr2 - sizeof(struct avail));
    
    // Calculate buddies
    struct avail* buddy1 = buddy_calc(&pool, block1);
    struct avail* buddy2 = buddy_calc(&pool, block2);
    
    // Verify buddies are correctly calculated
    TEST_ASSERT_NOT_NULL(buddy1);
    TEST_ASSERT_NOT_NULL(buddy2);
    
    buddy_free(&pool, ptr1);
    buddy_free(&pool, ptr2);
    buddy_destroy(&pool);
}


int main(void) {
    time_t t;
    unsigned seed = (unsigned)time(&t);
    fprintf(stderr, "Random seed:%d\n", seed);
    srand(seed);
    printf("Running extended memory tests.\n");

    UNITY_BEGIN();
    
    // Original tests
    RUN_TEST(test_buddy_init);
    RUN_TEST(test_buddy_malloc_one_byte);
    RUN_TEST(test_buddy_malloc_one_large);
    
    // New tests
    RUN_TEST(test_buddy_null_inputs);
    RUN_TEST(test_buddy_size_boundaries);
    RUN_TEST(test_buddy_realloc_comprehensive);
    RUN_TEST(test_buddy_fragmentation);
    RUN_TEST(test_buddy_calc_functionality);
    
    return UNITY_END();
}