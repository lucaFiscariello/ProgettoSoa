#ifndef READWRITEBLOCK
#define READWRITEBLOCK

#define ERAS 3
#define POS_META_BLOCK 2
#define DIM_META_DATA 12
#define DIM_DATA_BLOCK 4096 - DIM_META_DATA
#define DIM_BLOCK 4096
#define MAX_INVALID_BLOCK 800

#define concat_data(data,temp_block)\
   strcat(data,temp_block->data);\
   strcat(data,"\n");\


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


void inizialize_meta_block(void); //Alloca il terzo blocco sul device
void set_block_device_onMount(char* devname);
void set_block_device_onUmount(void);
void flush_device_metablk(void); // Scrive il metablocco sul device
int get_next_free_block(void);
struct meta_block_rcu* read_ram_metablk(void); // Legge metablocco in ram se presente
struct meta_block_rcu* read_device_metablk(void); // Legge metablocco dal device e lo salva in ram
void read(int block_to_read,struct block* block); 
void read_all_block(char* data); 
void write(int block_to_write,struct block* block);


#endif