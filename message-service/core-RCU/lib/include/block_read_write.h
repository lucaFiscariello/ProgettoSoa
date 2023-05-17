#ifndef READWRITEBLOCK
#define READWRITEBLOCK

#include "meta_block.h"
#include "rcu.h"

#define ENODATA -2
#define LOCKERROR -3
#define BHERROR -5
#define concat_data(data,temp_block)\
   strcat(data,temp_block->data);\
   strcat(data,"\n");\

#define check_return_read(ret)\
   if(ret == ENODATA)\
      return ENODATA;
      
#define check_return_read_and_unlock(ret,lock_element)\
   if(ret < 0){\
      unlock(lock_element);\
      return 0;\
     }


#define check_return_write_and_unlock(ret,lock_element)\
   if(ret < 0){\
      unlock(lock_element);\
      return ret;\
   }

#define check_block_validity(block_to_read,meta_block_rcu)\
   if(is_invalid(block_to_read,meta_block_rcu))\
        return ENODATA;

#define check_block_index(block,meta_block_rcu)\
   if(block<0 || block>meta_block_rcu->blocksNumber)\
        return ENODATA;

#define check_buffer_head(bh)\
   if(!bh)\
      return BHERROR;

#define check_bh_and_unlock(bh)\
   if(!bh){\
      rcu_read_unlock();\
      return BHERROR;\
   }


int get_next_free_block(void);
int read(int block_to_read,struct block* block); 
int read_all_block(char* data); 
int write(int block_to_write,struct block* block);


#endif
