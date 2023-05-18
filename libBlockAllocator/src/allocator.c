#include <stdio.h>
#include "allocator.h"
#include <stdlib.h>
#include <memory.h>

/* ToDo: Add module desc & doxygen dock */
/* ToDo: Warning! OS can prevent inter-process memory sharing theoretically */

int ypool_init(ypool_STC *pool){
    /* prevent already inited pool */
    if (pool->start_PTR != NULL)
        return -EALREADY;
    
    pthread_mutex_init(&pool->mutex, NULL); //fixme check all pool usage inside mutex locked block
    pthread_mutex_lock(&pool->mutex);

    pool->start_PTR = malloc(pool->pool_size);

    //fixme: check retval

    pool->allocate_PTR = pool->start_PTR;


    pthread_mutex_unlock(&pool->mutex);

    yformat(pool);

    return 0;
}

int yalloc_block(ypool_STC *pool, AD_POINTER *user_block){
    int ret = 0;

    /* using this mid buff we can prevent next pointer damaged by user situation */
    union yblock_UNT *block; 

    if (pool == NULL)
        return -EFAULT;

    pthread_mutex_lock(&pool->mutex); //fixme: resolve situation with double mutex lock

    if (pool->start_PTR == NULL)
    {
        ret = -EINVAL; /* Wrong pool */
        goto error;
    }

    /* prevent NOT end of pool */
    if(pool->allocate_PTR == NULL)
    {
        ret = -ENOMEM; /* No memory in pool */
        goto error;
    }

    block = (union yblock_UNT*) pool->allocate_PTR;

    // Throw memory to user 
    *user_block = pool->allocate_PTR; /* ToDo: clear block if needed */

    // and user can damage pointer in data
    // But we have an a copy

    /* move pointer to new block */
    pool->allocate_PTR = block->next_block;

    error:
    pthread_mutex_unlock(&pool->mutex); //fixme retcode??
    return ret;
}

int yfree_block(ypool_STC *pool, AD_POINTER *user_block){
    int ret;
    union yblock_UNT *block;
    AD_POINTER prev_allocate_PTR; 

    ret = ypool_check(pool);

    if(ret != 0)
        return ret;

    if (!yblock_belongs_to_pool(pool,user_block))
        return -EXDEV; /* ToDo: replace with special codes */

    /* ToDo: check if block allocated */
    /* Unable to know... */

    pthread_mutex_lock(&pool->mutex);

    /* save previous free pool block pointer */
    prev_allocate_PTR = pool->allocate_PTR;

    /* return user block to the start of the pool chain */
    pool->allocate_PTR = user_block;

    /* clear block if needed */
    /* memset( ... )*/

    /* write previous allocate pointer to returned block */
    block = (union yblock_UNT*) pool->allocate_PTR;
    block->next_block = prev_allocate_PTR;

    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

static int yformat(ypool_STC *pool){
    union yblock_UNT   *block;
    AD_POINTER          block_PTR;
    uint32_t            curr_block_num;

    //fixme: add pool validator func
    if (pool == NULL)
        return -EFAULT;

    if (pool->start_PTR == NULL)
        return -EINVAL;

    //fixme: prevent format while memory is allocated to user
    pthread_mutex_lock(&pool->mutex);
    block_PTR = pool->start_PTR;

    for (curr_block_num=0; curr_block_num < (pool->pool_size/pool->block_size)-1; curr_block_num++)
    {
        /* decode current block */
        block = (union yblock_UNT*) block_PTR;
        /* set pointer to next block */
        block->next_block = block_PTR+pool->block_size;

        block_PTR += pool->block_size;
    }

    /* set next as NULL for last block */
    block = (union yblock_UNT*) block_PTR;
    block->next_block = NULL;

    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

static bool yblock_belongs_to_pool(ypool_STC *pool, AD_POINTER *user_block)
{
    AD_POINTER    * pool_end;
    size_t         blocks_in_pool;

    if(ypool_check(pool) != 0)
        return false;

    blocks_in_pool = pool->pool_size/pool->block_size;

    pool_end = pool->start_PTR + pool->block_size * (blocks_in_pool-1); /* pool_end points to first byte of the last block (not to the last byte of the pool)  */

    return (pool->start_PTR <= *user_block && user_block <= pool_end); /* true if user block is between pool edges */
}

static int ypool_check(ypool_STC *pool){
    if (pool == NULL)
        return -EFAULT;

    if (pool->start_PTR == NULL)
        return -EINVAL;

    // what else needs to be checked? (& how much to stay fast)

    return 0;
}

/* Debug funcs */

int ypool_print(ypool_STC *pool){
    int           ret;
    AD_POINTER    block_PTR;
    size_t        blocks_in_pool;

    ret = ypool_check(pool);

    if(ret != 0)
        return ret;

    block_PTR = pool->start_PTR;
    blocks_in_pool = pool->pool_size/pool->block_size;

    /* walking for blocks in pool  */
    pthread_mutex_lock(&pool->mutex);
    for (uint32_t curr_block=0; curr_block < blocks_in_pool; curr_block++)
    {
        printf("========[block: 0x%016x]=========\n", block_PTR);
        _yblock_print(block_PTR, pool->block_size);
        printf("============================================\n");
        block_PTR += pool->block_size;
    }
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

/* !!! mutex unsecure func! */
static int _yblock_print(union yblock_UNT* block, size_t block_size){

    printf("block->next_block= 0x%016x\n\n",block->next_block);
    for (size_t i = 0; i < block_size; i++)
    {
        printf("block->data[%zu]=0x%x\n", i, (uint8_t)block->data[i]);
    }
    return 0;
}