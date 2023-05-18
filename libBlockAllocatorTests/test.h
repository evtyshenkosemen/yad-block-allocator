#ifndef TEST_H
#define TEST_H
#include "../libBlockAllocator/bin/allocator.h"
#include "../libBlockAllocator/bin/autoconf.h"

#define VERBOSE 0

/* multithreading mode */
#define BLOCK_ALLOC_RETRY       100 /* how many retries neededed to thread for allocate block */
#define THREADS_COUNT            20 /* how many threads will use our allocator asynchronously */
#define ALLOC_RETRY_WAIT_US    5000 /* how long wait to re-request block allocation */

/* funcs */
int test_yalloc_single_thread();
int test_yalloc_multithreaded(int threads);  
int emulate_pool_usage(ypool_STC * ypool);

/* data used to test one block */
#define TEST_SET_SIZE 20
uint8_t test_set[TEST_SET_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xAD, 0xAD, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0xAF, 0xCA};

#endif /* TEST_H */