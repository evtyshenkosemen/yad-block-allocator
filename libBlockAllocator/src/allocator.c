#include <stdio.h>
#include "allocator.h"
#include <stdlib.h>
#include <memory.h>

/**
    \file
    \brief Block allocator library

    \todo Warning! OS can prevent inter-process memory sharing theoretically
*/

/**
    \brief 
        Used to allocate memory from the pool

    \param[in/out] pool Pointer to the pool to which the operation will be applied

    \return 
        -EFAULT    - Pool pointer is NULL
        -EALREADY  - Pool is initialized already
        -ENOMEM    - There are no free memory in system to allocate it for requested pool
                 0 - Successfuly initialized pool
*/

int ypool_init(ypool_STC *pool){
    size_t    blocks_in_pool;
    int       ret;

    if (pool == NULL)
        return -EFAULT;

    /* prevent already inited pool */
    if (pool->start_PTR != NULL)
        return -EALREADY;

    blocks_in_pool = pool->pool_size/pool->block_size;
    
    pthread_mutex_init(&pool->mutex, NULL); //fixme check all pool usage inside mutex locked block
    pthread_mutex_lock(&pool->mutex);

    pool->start_PTR = malloc( blocks_in_pool * (sizeof(AD_POINTER)+pool->block_size) );

    if(pool->start_PTR == NULL)
        return -ENOMEM;

    pool->next_free_block_PTR = pool->start_PTR;


    pthread_mutex_unlock(&pool->mutex);

    yformat(pool);

    return 0;
}

/**
    \brief 
        Used to allocate block of memory from the pool

    \param[in] pool Pointer to the pool to which the operation will be applied
    \param[out] *user_block Pointer to allocated memory

    \return 
        -EFAULT    - Pool pointer is NULL
        -EINVAL    - Pool has no pointer to the beginning
        -ENOMEM    - Pool has no free memory
                 0 - Successfuly freed user_block
*/

int yalloc_block(ypool_STC *pool, AD_POINTER *user_block){
    int ret = 0;

    /* using this mid buff we can prevent next pointer damaged by user situation */
    yblock_STC *block;
    AD_POINTER allocate_PTR;
    AD_POINTER next_free_block_PTR; 

    if (pool == NULL)
        return -EFAULT;

    pthread_mutex_lock(&pool->mutex); //fixme: resolve situation with double mutex lock

    if (pool->start_PTR == NULL)
    {
        ret = -EINVAL; /* Wrong pool */
        goto error;
    }

    /* prevent end of pool */
    if(pool->next_free_block_PTR == NULL)
    {
        ret = -ENOMEM; /* No memory in pool */
        goto error;
    }

    block = (yblock_STC*) pool->next_free_block_PTR;
    /* save pointer from allocating block */
    next_free_block_PTR = block->next_block;

    // Throw memory to user
    allocate_PTR = pool->next_free_block_PTR;
    
    allocate_PTR += sizeof(AD_POINTER); /* skip next block pointer (give user memory starting right after next block pointer) */

    /* mark curr block as allocated (set next_block = NULL) */
    *(AD_POINTER*)pool->next_free_block_PTR = NULL;

    *user_block = allocate_PTR; /* ToDo: clear block if needed */

    // and user can damage pointer in data
    // But we have an a copy

    /* move pointer to new block */
    pool->next_free_block_PTR = next_free_block_PTR;

    error:
    pthread_mutex_unlock(&pool->mutex); //fixme retcode??
    if (DEBUG) printf("yalloc_block ret = %d\n",ret);
    return ret;
}

/**
    \brief 
        Used to free single user block & return it to the pool

    \param[in] pool Pointer to the pool to which the operation will be applied
    \param[in] *user_block Block which needed to free

    \return 
        -EFAULT    - Pool pointer is NULL
        -EINVAL    - Pool has no pointer to the beginning
        -EXDEV     - user_block is not belong the pool
        -EALREADY  - user_block is marked as free
                 0 - Successfuly freed user_block
*/

int yfree_block(ypool_STC *pool, AD_POINTER *user_block){
    int ret;
    yblock_STC *block;
    AD_POINTER user_block_PTR;
    yblock_STC *returned_block;

    ret = ypool_check(pool);

    if(ret != 0){
        goto error;
    }

    pthread_mutex_lock(&pool->mutex);

    if (!yblock_belongs_to_pool(pool,user_block)){
        ret =  -EXDEV; /* ToDo: replace with special codes */
        goto error;
    }

    /* shift back returned block to PTR size (to correct work with yblock_STC) */
    user_block_PTR = user_block;
    user_block_PTR -= sizeof(AD_POINTER);

    /* decode returned block */
    returned_block = (yblock_STC*) user_block_PTR;

    /* Check if block allocated */
    if (returned_block->next_block != NULL){ /* for allocated block next_block ptr must be NULL */
        ret = -EALREADY; /* block is not allocated (already free) */
        goto error;
    }

    /* write previous allocate pointer to returned block */    
    returned_block->next_block = pool->next_free_block_PTR;
    
    /* set pool pointer to returned block */
    pool->next_free_block_PTR = user_block_PTR;

    error:
    pthread_mutex_unlock(&pool->mutex); //fixme retcode??
    if(DEBUG) printf("yfree ret=%d\n", ret);
    return ret;
}

/**
    \brief 
        Used to format initiated pool to singly linked list

    \param[in] pool Pointer to the pool to which the operation will be applied

    \return 
        -EFAULT - Pool pointer is NULL
        -EINVAL - Pool has no pointer to the beginning
              0 - On success
*/

static int yformat(ypool_STC *pool){
    yblock_STC        * block;
    AD_POINTER          block_PTR;
    uint32_t            curr_block_num;
    int                 ret;
    size_t              blocks_in_pool;

    /* 1. Check the pool for validity */
    ret = ypool_check(pool);

    if(ret != 0)
        return ret;
    /* 2. reserved */
    //fixme: prevent format while memory is allocated to user

    /* 3. Lock pool */ 
    pthread_mutex_lock(&pool->mutex);
    /* 4. Set block_PTR to the beginning of the pool */
    block_PTR = pool->start_PTR;

    blocks_in_pool = pool->pool_size/pool->block_size;

    /* */
    for (curr_block_num=0; curr_block_num < blocks_in_pool-1; curr_block_num++)
    {
        /* decode current block */
        block = (yblock_STC*) block_PTR;
        /* set pointer to next block */
        block->next_block = block_PTR + sys_block_size(pool->block_size);

        block_PTR += sys_block_size(pool->block_size);
    }

    /* set next as NULL for last block */
    block = (yblock_STC*) block_PTR;
    block->next_block = NULL;

    pthread_mutex_unlock(&pool->mutex);

    return 0;
}

/**
    \brief 
        Checks belonging of a user block to the pool

    \details

    \param[in] pool Pointer to the pool to which the operation will be applied

    \return 
        false - Not belongs to the pool
        true  - Belongs to the pool
*/

static bool yblock_belongs_to_pool(ypool_STC *pool, AD_POINTER user_block)
{
    AD_POINTER      lowest_user_pointer;
    AD_POINTER      highest_user_pointer;
    size_t          blocks_in_pool;

    if(ypool_check(pool) != 0)
        return false;

    blocks_in_pool = pool->pool_size/pool->block_size;

    lowest_user_pointer = pool->start_PTR + sizeof(AD_POINTER);
    highest_user_pointer = lowest_user_pointer + (sys_block_size(pool->block_size) * (blocks_in_pool-1));

    if (DEBUG) printf("#ybbtp pool->start_PTR=0x%x\n",pool->start_PTR);
    if (DEBUG) printf("#ybbtp highest_user_pointer=0x%x\n",highest_user_pointer);
    if (DEBUG) printf("#ybbtp user_block=0x%x\n",user_block);
    if (DEBUG) printf("#ybbtp lowest_user_pointer=0x%x\n",lowest_user_pointer);

    return (lowest_user_pointer <= user_block && user_block <= highest_user_pointer); /* true if user block is between pool edges */
}

/**

    \brief 
        Validate pool

    \details
        1. Check that pool PTR is not NULL
        2. Check that start_PTR is not NULL
        3. reserved

    \param[in] pool Pointer to the pool to which the operation will be applied

    \return 
        -EFAULT - Pool pointer is NULL
        -EINVAL - Pool has no pointer to the beginning
              0 - Pool correct

*/

static int ypool_check(ypool_STC *pool){
    if (pool == NULL)
        return -EFAULT;

    if (pool->start_PTR == NULL)
        return -EINVAL;

    // what else needs to be checked? (& how much to stay fast)

    return 0;
}

/**
    \brief Get system block size using user block size
    \param[in] user_block_size User block size
    \return System block size (user accessed memory + pointer memory)
*/

static size_t sys_block_size(size_t user_block_size){
    return user_block_size + sizeof(AD_POINTER);
}

/* Debug funcs */
#if DEBUG == 1

/**
    \brief Print decoded pool data
    \param[in] pool Pointer to the pool to which the operation will be applied
    \return 
        -EFAULT - Pool pointer is NULL
        -EINVAL - Pool has no pointer to the beginning
              0 - On success
*/
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
        block_PTR += sys_block_size(pool->block_size);
    }
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

/**
    \brief Print decoded block data [Warning!!! mutex unsecure func!]
    \param[in] block Pointer to the block to which the operation will be applied
    \param[in] user_block_size User block size
    \return 0
*/
static int _yblock_print(yblock_STC* block, size_t user_block_size){

    printf("block->next_block= 0x%016x\n\n",block->next_block);
    for (size_t i = 0; i < user_block_size; i++)
    {
        printf("block->data[%zu]=0x%x\n", i, (uint8_t)block->data[i]);
    }
    return 0;
}

/**
    \brief Print raw pool data
    \param[in] pool Pointer to the pool to which the operation will be applied
    \return 
        -EFAULT - Pool pointer is NULL
        -EINVAL - Pool has no pointer to the beginning
              0 - On success
*/
int ypool_print_raw(ypool_STC *pool){
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
        _sysblock_print_raw(block_PTR, sys_block_size(pool->block_size));
        printf("============================================\n");
        block_PTR += sys_block_size(pool->block_size);
    }
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

/**
    \brief Print raw block data [Warning!!! mutex unsecure func!]
    \param[in] block Pointer to the data
    \param[in] block_size Size in bytes which needed to print
    \return 0
*/
static int _sysblock_print_raw(AD_POINTER data, size_t block_size){

    uint8_t * data_PTR;

    data_PTR = data;

    printf("===========PTR=0x%x=========================\n",data);
    for (size_t i = 0; i < block_size; i++)
    {
        printf("data[%zu]=0x%x\n", i, data_PTR[i]);
    }
    printf("============================================\n");
    return 0;
}

#endif