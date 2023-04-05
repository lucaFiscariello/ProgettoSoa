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

void read_block_rcu(int block_to_read,struct block* block);
void read_all_block_rcu(char* block_data);
int write_rcu(char* block_data);
//void invalidate_rcu(int block_to_invalidate);

#endif