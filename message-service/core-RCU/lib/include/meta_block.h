#ifndef META
#define META

#include <linux/blkdev.h>
#include "../../../singlefile-FS/lib/include/singlefilefs.h"

#define MAX_INVALID_BLOCK (DIM_BLOCK-88)/4
#define BIT_INT 32
#define POS_META_BLOCK 2
#define POS_I_NODE 1
#define DIM_META_DATA 12
#define DIM_DATA_BLOCK 4096 - DIM_META_DATA
#define DIM_BLOCK 4096
#define INIZIALIZE_META_BLK 1
#define BLOCK_ERROR -1
#define BHERROR -5
#define COPYERROR -15
#define ZERO_WRITER 0
#define LOCK_WRITER 1
#define MAX_BLOCK 32000
#define EPOCHS 2
#define MASK 0x0000000000000001

#define check_mount(metablock)\
    if(!__sync_fetch_and_add(&metablock->mount_state,0))\
        return ENODEV;

#define change_state_mount(meta_block) ( __sync_fetch_and_xor(&meta_block->mount_state,MASK) )

#define set_bit(pos,meta_block)      ( meta_block->bitMap[pos/BIT_INT] |=  (1 << (pos%BIT_INT)) )
#define is_invalid(pos,meta_block)   ( meta_block->bitMap[pos/BIT_INT] &   (1 << (pos%BIT_INT)) )
#define clear_bit(pos,meta_block)    ( meta_block->bitMap[pos/BIT_INT] &= ~(1 << (pos%BIT_INT)) )

#define check_buffer_head(bh)\
   if(!bh)\
      return BHERROR;

#define check_bh_and_unlock(bh,epoch)\
   if(!bh){\
      rcu_unlock_read(epoch);\
      return BHERROR;\
   }

/**
 * Tale struttura implementa il "meta blocco" ovvero un blocco di dati memorizzato nel device è che mantiene una serie di 
 * informazioni utili per getire la scrittura e la lettura concorrente di altri blocchi nel device. 
*/
struct meta_block_rcu {

   /*Id del primo blocco valido scritto sul device*/
   int firstBlock;

   /*Id del prossimo blocco libero su cui è possibile effettuare operazioni di scrittura*/
   int nextFreeBlock;

   /*Id dell'ultimo blocco su cui sono state effettuate operazioni di scrittura*/
   int lastWriteBlock;

   /*bitMap dei blocchi. Permette di capire quali blocchi sono stati invalidati*/
   int bitMap[MAX_INVALID_BLOCK];

   /*Linked list di tutti i blocchi invalidati.*/
   struct invalid_block * headInvalidBlock;

   /*Usato per implementare lock in scrittura*/
   int write_lock;

   /*Numero di blocchi che compongono il device*/
   int blocksNumber;

   /*Numero di blocchi invalidati*/
   int invalidBlocksNumber;

   /*Campo che permette di capire se il metablocco è stato formattato correttamente sul device*/
   int already_inizialize;

   int epoch;

   int standing[EPOCHS];

   int counter_active_thread;

   int mount_state;

} ; 


/**
 * Struttura che implementa il blocco che può essere letto o scritto sul device
*/
struct block {

   /*Validità del blocco*/
   int validity;

   /*Id del blocco scritto temporalmente dopo al blocco corrente*/
   int next_block;

   /*Id del blocco scritto temporalmente prima del blocco corrente*/
   int pred_block;

   /*Dati utili memorizzabili nel device*/
   char data[DIM_DATA_BLOCK];
};


/**
 * Struttura utilizzata per implementare la linked list dei blocchi invalidati
*/
struct invalid_block {
   int block;
   struct invalid_block* next;
};

int inizialize_meta_block(void); 
void set_block_device_onMount(const char* devname);
void set_block_device_onUmount(void);
int increment_dim_file(int write_bytes);
int decrement_dim_file(int bytes);
struct block_device * get_block_device_AfterMount(void);
struct meta_block_rcu* read_ram_metablk(void); 
struct meta_block_rcu* read_device_metablk(void); 
int flush_device_metablk(void); 

int   rcu_lock_read(void);
void  rcu_unlock_read(int last_epoch);
void rcu_synchronize(void);
void update_epoch(void);
void  increment_active_threads(void);
void  decrement_active_threads(void);
void wait_umount(void);

#endif
