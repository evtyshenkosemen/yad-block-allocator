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
    AD_POINTER         allocate_PTR;
    pthread_mutex_t    mutex;
    /* ToDo: mb implement free space (if needed quck free space info) */
}ypool_STC;

union yblock_UNT
{
    char          data[BLOCK_SIZE];    /* ToDo: Fixme replace with BLOCK_SIZE_MAX */
    AD_POINTER    next_block;          /* ToDo: Fixme! We wanted to use a block as next pointer but it limits min block size to pointer size (x64-8) */
};

/* public funcs */
int ypool_init(ypool_STC *pool);
int yalloc_block(ypool_STC *pool, AD_POINTER *block);
int yfree_block(ypool_STC *pool, AD_POINTER *user_block);

/* private funcs */
static int yformat(ypool_STC *pool);
static bool yblock_belongs_to_pool(ypool_STC *pool, AD_POINTER *user_block);
static int ypool_check(ypool_STC *pool);

/* debug */
int ypool_print(ypool_STC *pool);
static int _yblock_print(union yblock_UNT* block, size_t block_size);

#endif //ALLOCATOR_H