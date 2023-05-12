#ifndef READWRITEBLOCK
#define READWRITEBLOCK

#include "meta_block.h"
#include "rcu.h"

#define ENODATA -2
#define LOCKERROR -3
#define concat_data(data,temp_block)\
   strcat(data,temp_block->data);\
   strcat(data,"\n");\

#define check_return_read(ret)\
   if(ret == ENODATA)\
      return ENODATA;

#define check_block_validity(block_to_read,meta_block_rcu)\
   if(is_invalid(block_to_read,meta_block_rcu))\
        return ENODATA;

#define check_block_index(block,meta_block_rcu)\
   if(block<0 || block>meta_block_rcu->blocksNumber)\
        return ENODATA;


int get_next_free_block(void);
int read(int block_to_read,struct block* block); 
int read_all_block(char* data); 
int write(int block_to_write,struct block* block);


#endif