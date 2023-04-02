#ifndef READWRITEBLOCK
#define READWRITEBLOCK

#define POS_META_BLOCK 2
#define DIM_DATA_BLOCK 4072
#define DIM_BLOCK 4096

struct block {
   int arrayValidBlockEpoch[2];
   int arrayNextEpoch[2];
   int arrayPredEpoch[2];
   char data[DIM_DATA_BLOCK];
};


struct meta_block_rcu {
   int firstBlock;
   int epoch;
   int arrayEpochCounter[2];
   int validBlock;
   int nextFreeBlock;
   int lastWriteBlock;
   int invalidBlock[800];
} ; 

void read(char* devname, int block_to_read,struct block* block);
void write(char* devname, int block_to_write,struct block* block);
void read_meta_block(char* devname);
void write_meta_block(char* devname);

#endif