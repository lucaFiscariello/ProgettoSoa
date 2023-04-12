#ifndef META
#define META

#include <linux/blkdev.h>

#define MAX_INVALID_BLOCK 800
#define POS_META_BLOCK 2
#define DIM_META_DATA 12
#define DIM_DATA_BLOCK 4096 - DIM_META_DATA
#define DIM_BLOCK 4096
#define MAX_INVALID_BLOCK 800

struct meta_block_rcu {
   int firstBlock;
   int nextFreeBlock;
   int lastWriteBlock;
   int invalidBlocks[MAX_INVALID_BLOCK];
   struct invalid_block * headInvalidBlock;
   int write_lock;
   int blocksNumber;
   int invalidBlocksNumber;
} ; 

struct block {
   int validity;
   int next_block;
   int pred_block;
   char data[DIM_DATA_BLOCK];
};

struct invalid_block {
   int block;
   struct invalid_block* next;
};

void inizialize_meta_block(void); //Alloca il terzo blocco sul device
void set_block_device_onMount(char* devname);
void set_block_device_onUmount(void);
struct block_device * get_block_device_AfterMount(void);
struct meta_block_rcu* read_ram_metablk(void); // Legge metablocco in ram se presente
struct meta_block_rcu* read_device_metablk(void); // Legge metablocco dal device e lo salva in ram
void flush_device_metablk(void); // Scrive il metablocco sul device

#endif