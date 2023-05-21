#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include "autoconf.h"
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

typedef void* AD_POINTER; /* find bit depth independent pointer type */
typedef struct _ypool
{
    AD_POINTER         start_PTR;
    size_t             block_size;
    size_t             pool_size;
    AD_POINTER         next_free_block_PTR;
    pthread_mutex_t    mutex;
    /* ToDo: mb implement free space (if needed quck free space info) */
}ypool_STC;

typedef struct __attribute__((__packed__)) _yblock
{
    AD_POINTER    next_block;
    uint8_t       data[MAX_BLOCK_SIZE];
    /* Warning: Memory allocating NOT via sizeof(yblock_STC) to achieve minimal size. If you wanted to add some field in this struct - edit ypool_init() malloc */
}yblock_STC;

/* public funcs */
int ypool_init(ypool_STC *pool);
int yalloc_block(ypool_STC *pool, AD_POINTER *block);
int yfree_block(ypool_STC *pool, AD_POINTER *user_block);

/* private funcs */
static int yformat(ypool_STC *pool);
static bool yblock_belongs_to_pool(ypool_STC *pool, AD_POINTER user_block);
static int ypool_check(ypool_STC *pool);
static size_t sys_block_size(size_t user_block_size);

/* debug */
#if DEBUG == 1
int ypool_print(ypool_STC *pool);
static int _yblock_print(yblock_STC* block, size_t user_block_size);
static int _sysblock_print_raw(AD_POINTER data, size_t block_size);
#endif

#endif //ALLOCATOR_H