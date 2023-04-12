#ifndef RCU
#define RCU

#include "block_read_write.h"

#define ZERO_WRITER 0
#define LOCK_WRITER 1
#define VALID_BLOCK 1
#define INVALID_BLOCK 0
#define BLOCK_ERROR -1
#define lock(lock_element) (__sync_val_compare_and_swap(&lock_element,ZERO_WRITER,LOCK_WRITER))
#define unlock(lock_element) (lock_element=ZERO_WRITER)

#define update_pointer_onInvalidate(pred_block,block_update_next,next_block,block_update_pred,meta_block_rcu) \
    if(pred_block !=NULL) pred_block->next_block = block_update_next;\
    if(next_block !=NULL) next_block->pred_block = block_update_pred; \
    if(block_to_invalidate== meta_block_rcu->firstBlock){ \
        if(block_update_next == BLOCK_ERROR ){ meta_block_rcu->firstBlock = POS_META_BLOCK+1;}\
        else{meta_block_rcu->firstBlock = block_update_next;}\
    }\
    else if (block_to_invalidate == meta_block_rcu->lastWriteBlock){\
        meta_block_rcu->lastWriteBlock = block_update_pred;\
    }

void read_block_rcu(int block_to_read,struct block* block);
void read_all_block_rcu(char* block_data);
int write_rcu(char* block_data);
void invalidate_rcu(int block_to_invalidate);


#endif