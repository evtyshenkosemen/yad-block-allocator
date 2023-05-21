#include <stdio.h>
#include <memory.h>
#include <assert.h>
#include <unistd.h>

#include "test.h"

volatile int test_passed_threads = 0; /* Threads where block test passed */ /* p.s. is "add" atomic operation? */
ypool_STC global_pool = {NULL, BLOCK_SIZE, POOL_SIZE, NULL, 0};

int main(int argc, char const *argv[])
{
    printf("[tests] Start\n");

    assert(TEST_SET_SIZE >= BLOCK_SIZE);

    assert(test_yalloc_single_thread() == 0);
    assert(test_yalloc_multithreaded(THREADS_COUNT) == 0);
    
    printf("[tests] Successfuly end !\n");
    return 0;
}

int test_yalloc_single_thread(){    
    printf("[Single thread test] Start\n");

    ypool_STC pool_one = {NULL, BLOCK_SIZE, POOL_SIZE, NULL, 0};
    AD_POINTER our_block;
    int i;

    
    printf("   initializing pool\n");
    assert(ypool_init(&pool_one) == 0);
    printf("                                           Done!\n");

    printf("   testing %d blocks allocation\n", POOL_SIZE/BLOCK_SIZE);
    for(i = 0; i < POOL_SIZE/BLOCK_SIZE; i++){
        /* get & test single block */ /* ToDo: Maybe optimize */
        assert(yalloc_block(&pool_one, &our_block) == 0);
        if (VERBOSE) printf("         returned block: 0x%x\n", our_block);
        memset(our_block,0x00,BLOCK_SIZE);
        memcpy(our_block,test_set,BLOCK_SIZE);
        assert(memcmp(our_block, test_set, BLOCK_SIZE) == 0);
    }
    printf("                                           Done!\n");

    /* check is pool end correct */
    printf("   testing overbooking\n");
    assert(yalloc_block(&pool_one, &our_block) == -ENOMEM);
    printf("                                           Done!\n");
    
    /* check freeing */
    /* beleive that failed yallock_block not damaged our pointer */

    /* check out of pool bounds free op (upper) */
    printf("   test freeing of block which out the pool (up)\n");
    our_block+=(BLOCK_SIZE + sizeof(AD_POINTER)); /* make bigger than last allocated block */   
    assert(yfree_block(&pool_one, our_block) == -EXDEV);
    our_block-=(BLOCK_SIZE + sizeof(AD_POINTER));  /* make equal to last allocated block */
    printf("                                           Done!\n");

    printf("   freeing blocks\n");
    for(i = 0; i < POOL_SIZE/BLOCK_SIZE; i++){ /* walking to pool start*/
        /* free single block */
        if (VERBOSE) printf("         requesting free block 0x%x op\n", our_block);
        assert(yfree_block(&pool_one, our_block) == 0);

        our_block-=(BLOCK_SIZE + sizeof(AD_POINTER));
    }
    printf("                                           Done!\n");

    /* test out of pool bounds free operation (upper) */
    printf("   test freeing of block which out the pool (down)\n");
    if (VERBOSE) printf("         requesting free block 0x%x op\n", our_block);
    assert(yfree_block(&pool_one, our_block) == -EXDEV);
    printf("                                           Done!\n");

    /* test free operation on already freed block */
    our_block+=(BLOCK_SIZE + sizeof(AD_POINTER)); /* point to 1st block in pool */
    printf("   test free operation on already freed block\n");
    if (VERBOSE) printf("         requesting free block 0x%x op\n", our_block);
    assert(yfree_block(&pool_one, our_block) == -EALREADY);
    printf("                                           Done!\n");
    printf("[Single thread test] Passed!\n");

    return 0;
}

void *test_thread(void *vargp)
{
    if(emulate_pool_usage(&global_pool) == 0)
        test_passed_threads++; /* add counter if */

    return NULL;
}


/* return: 0 if block allocated & tested 
           1 if unable to get block for entire retry time
*/
int emulate_pool_usage(ypool_STC * ypool)
{
    AD_POINTER our_block;
    int ret;
    int retry = BLOCK_ALLOC_RETRY;

    /* get & test single block */
    ret = yalloc_block(ypool, &our_block);

    assert(ret == 0 || ret == -ENOMEM);

    /* Realloc block if not allocated yet */
    while (ret != 0 && retry > 0)
    {
        usleep(ALLOC_RETRY_WAIT_US);
        ret = yalloc_block(ypool, &our_block);
        retry--;
    }

    /* catch so long getting block */
    if (retry <= 0)
    {
        printf("                                                                          [thr:0x%x] unable to alloc block. Termnating...\n",pthread_self());
        return -1;
    }

    /* test allocated block with random delays */
    if (VERBOSE) printf("                                                                          [thr:0x%x]got block: 0x%x with %d retries\n",pthread_self(), our_block, BLOCK_ALLOC_RETRY-retry);
    usleep(((rand() % 20)+1) * 1000); /* 1000-20000 ms */
    memset(our_block,0x00,BLOCK_SIZE);
    usleep(((rand() % 20)+1) * 1000); /* 1000-20000 ms */
    memcpy(our_block,test_set,BLOCK_SIZE); /* ToDo: add pseudo-rand dataset here */
    usleep(((rand() % 20)+1) * 1000); /* 1000-20000 ms */
    assert(memcmp(our_block, test_set, BLOCK_SIZE) == 0);
    if (VERBOSE) printf("                                                                          [thr:0x%x]tested block: 0x%x\n",pthread_self(), our_block);
    if (VERBOSE) printf("                                                                          [thr:0x%x]trying to free block 0x%x\n",pthread_self(), our_block);
    assert(yfree_block(ypool, our_block) == 0);
    if (VERBOSE) printf("                                                                          [thr:0x%x]successfully freed block 0x%x\n",pthread_self(), our_block);

    return 0;
}



int test_yalloc_multithreaded(int threads){    
    printf("\n[Multithreading test] Start in %d threads\n", threads);

    
    printf("   initializing global pool\n");
    assert(ypool_init(&global_pool) == 0);
    printf("                                           Done!\n");

    printf("   checking pool reinitiating\n");
    assert(ypool_init(&global_pool) == -EALREADY);
    printf("                                           Done!\n");

    pthread_t * thread_id;
    thread_id = malloc(sizeof(pthread_t)*threads);
    int i;

    printf("   сreating %d threads\n", threads);
    for(i = 0; i < threads; i++){
        pthread_create(&thread_id[i], NULL, test_thread, NULL);
        if (VERBOSE) printf("         [i=%d]spawning thread 0x%x\n",i, thread_id[i]);
    }
    printf("                                           Done!\n");


    printf("   waiting %d threads end\n\n", threads);
    for(i = 0; i < threads; i++){
        if (VERBOSE) printf("         [i=%d]spawning thread 0x%x\n",i, thread_id[i]);
        pthread_join(thread_id[i], NULL);
    }

    printf("[Multithreading test] Test end!  %d/%d got & test blocks!\n",test_passed_threads, threads);

    if(test_passed_threads < threads/2){
        printf("[Multithreading test] Warning! Less than 1/2 of threads are tested blocks!\n Pay attention to this situation! Increase POOL_SIZE/ALLOC_RETRY_WAIT OR decrease THREADS_COUNT.\n");
        return -1;
    }
        
    return 0;
}

