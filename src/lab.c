 #include "lab.h"
#include <sys/mman.h>
#include <errno.h>

size_t btok(size_t bytes) {
    if (bytes == 0) return 0;
    
    size_t k = 0;
    size_t size = UINT64_C(1);
    
    // Find smallest k where 2^k >= bytes
    while (size < bytes) {
        size = size << 1;
        k++;
    }
    
    return k;
}

struct avail *buddy_calc(struct buddy_pool *pool, struct avail *block) {
    if (!pool || !block) return NULL;
    
    // Calculate offset from base
    uintptr_t offset = (uintptr_t)block - (uintptr_t)pool->base;
    
    // Calculate buddy's offset using XOR with block size
    uintptr_t buddy_offset = offset ^ (UINT64_C(1) << block->kval);
    
    // Return buddy address
    return (struct avail *)((uintptr_t)pool->base + buddy_offset);
}

void buddy_init(struct buddy_pool *pool, size_t size) {
    if (!pool) return;

    // Calculate k value needed for requested size
    size_t k = btok(size);
    
    // Ensure k is within bounds
    if (k < MIN_K) k = MIN_K;
    if (k > MAX_K - 1) k = MAX_K - 1;
    
    // Calculate actual size (power of 2)
    size = UINT64_C(1) << k;
    
    // Initialize pool metadata
    pool->kval_m = k;
    pool->numbytes = size;
    
    // Initialize avail array
    for (size_t i = 0; i <= MAX_K - 1; i++) {
        pool->avail[i].tag = BLOCK_UNUSED;
        pool->avail[i].kval = i;
        pool->avail[i].next = &pool->avail[i];
        pool->avail[i].prev = &pool->avail[i];
    }
    
    // Map memory
    pool->base = mmap(NULL, size, PROT_READ | PROT_WRITE, 
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (pool->base == MAP_FAILED) {
        errno = ENOMEM;
        return;
    }
    
    // Initialize base block
    struct avail *base = pool->base;
    base->tag = BLOCK_AVAIL;
    base->kval = k;
    base->next = &pool->avail[k];
    base->prev = &pool->avail[k];
    
    // Link base block into avail array
    pool->avail[k].next = base;
    pool->avail[k].prev = base;
}

void *buddy_malloc(struct buddy_pool *pool, size_t size) {
    if (!pool || size == 0) return NULL;
    
    // Calculate required k value including header size
    size_t required_k = btok(size + sizeof(struct avail));
    if (required_k < SMALLEST_K) required_k = SMALLEST_K;
    
    // Don't allow allocations larger than the pool
    if (required_k > pool->kval_m) {
        errno = ENOMEM;
        return NULL;
    }
    
    // Find smallest available block that fits
    size_t k = required_k;
    while (k <= pool->kval_m) {
        if (pool->avail[k].next != &pool->avail[k]) {
            // Found a block
            struct avail *block = pool->avail[k].next;
            
            // Remove from avail list
            block->next->prev = block->prev;
            block->prev->next = block->next;
            
            // Split block if necessary
            while (k > required_k) {
                k--;
                
                // Calculate buddy
                struct avail *buddy = (struct avail *)((uintptr_t)block + (UINT64_C(1) << k));
                
                // Initialize buddy
                buddy->tag = BLOCK_AVAIL;
                buddy->kval = k;
                buddy->next = &pool->avail[k];
                buddy->prev = pool->avail[k].prev;
                
                // Add buddy to avail list
                pool->avail[k].prev->next = buddy;
                pool->avail[k].prev = buddy;
                
                // Update block for next split
                block->kval = k;
            }
            
            // Mark block as reserved
            block->tag = BLOCK_RESERVED;
            
            // Return usable memory (after header)
            return (void *)((char *)block + sizeof(struct avail));
        }
        k++;
    }
    
    errno = ENOMEM;
    return NULL;
}

void buddy_free(struct buddy_pool *pool, void *ptr) {
    if (!pool || !ptr) return;
    
    // Get block header
    struct avail *block = (struct avail *)((char *)ptr - sizeof(struct avail));
    
    // Validate block
    if (block->tag != BLOCK_RESERVED) return;
    
    // Mark as available
    block->tag = BLOCK_AVAIL;
    
    // Try to merge with buddy
    while (block->kval < pool->kval_m) {
        struct avail *buddy = buddy_calc(pool, block);
        
        // Check if buddy is available for merging
        if (!buddy || buddy->tag != BLOCK_AVAIL || buddy->kval != block->kval) break;
        
        // Remove buddy from its avail list
        buddy->next->prev = buddy->prev;
        buddy->prev->next = buddy->next;
        
        // Determine which block becomes the parent
        if (block > buddy) {
            struct avail *temp = block;
            block = buddy;
            buddy = temp;
        }
        
        // Increase block size
        block->kval++;
    }
    
    // Add block to appropriate avail list
    block->next = &pool->avail[block->kval];
    block->prev = pool->avail[block->kval].prev;
    pool->avail[block->kval].prev->next = block;
    pool->avail[block->kval].prev = block;
}

void *buddy_realloc(struct buddy_pool *pool, void *ptr, size_t size) {
    if (!pool) return NULL;
    
    // Handle special cases
    if (!ptr) return buddy_malloc(pool, size);
    if (size == 0) {
        buddy_free(pool, ptr);
        return NULL;
    }
    
    // Get current block info
    struct avail *block = (struct avail *)((char *)ptr - sizeof(struct avail));
    size_t old_size = (UINT64_C(1) << block->kval) - sizeof(struct avail);
    
    // Allocate new block
    void *new_ptr = buddy_malloc(pool, size);
    if (!new_ptr) return NULL;
    
    // Copy data
    size_t copy_size = (size < old_size) ? size : old_size;
    char *src = ptr;
    char *dst = new_ptr;
    for (size_t i = 0; i < copy_size; i++) {
        dst[i] = src[i];
    }
    
    // Free old block
    buddy_free(pool, ptr);
    
    return new_ptr;
}

void buddy_destroy(struct buddy_pool *pool) {
    if (!pool || !pool->base) return;
    
    munmap(pool->base, pool->numbytes);
    pool->base = NULL;
}
