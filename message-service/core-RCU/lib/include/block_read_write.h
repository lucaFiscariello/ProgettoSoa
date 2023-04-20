#ifndef READWRITEBLOCK
#define READWRITEBLOCK

#include "meta_block.h"
#include "rcu.h"

#define concat_data(data,temp_block)\
   strcat(data,temp_block->data);\
   strcat(data,"\n");\


int get_next_free_block(void);
void read(int block_to_read,struct block* block); 
void read_all_block(char* data); 
void write(int block_to_write,struct block* block);


#endif