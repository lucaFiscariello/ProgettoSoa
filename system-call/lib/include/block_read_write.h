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
   int write_lock;
   int blockNumber;
} ; 


void inizialize_meta_block(void); //Alloca il terzo blocco sul device
void set_block_device_onMount(char* devname);
void set_block_device_onUmount(void);
void flush_device_metablk(void); // Scrive il metablocco sul device
struct meta_block_rcu* read_ram_metablk(void); // Legge metablocco in ram se presente
struct meta_block_rcu* read_device_metablk(void); // Legge metablocco dal device e lo salva in ram
void read(int block_to_read,struct block* block); 
void write(int block_to_write,struct block* block);


#endif