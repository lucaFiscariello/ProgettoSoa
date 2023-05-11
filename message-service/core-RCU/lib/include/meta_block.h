#ifndef META
#define META

#include <linux/blkdev.h>
#include "rcu.h"
#include "../../../singlefile-FS/lib/include/singlefilefs.h"

#define MAX_INVALID_BLOCK 800
#define POS_META_BLOCK 2
#define POS_I_NODE 1
#define DIM_META_DATA 12
#define DIM_DATA_BLOCK 4096 - DIM_META_DATA
#define DIM_BLOCK 4096
#define MAX_INVALID_BLOCK 800
#define ENODEV -4

#define check_mount()\
    if(get_block_device_AfterMount()==NULL)\
        return ENODEV;


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

   /*Array di id di tutti i blocchi invalidati*/
   int invalidBlocks[MAX_INVALID_BLOCK];

   /**
    * Linked list di tutti i blocchi invalidati. Ha la stessa responsabilità di "invalidBlocks[MAX_INVALID_BLOCK]"
    * ma è utilizzata per motivi prestazionali
   */
   struct invalid_block * headInvalidBlock;

   /*Usato per implementare lock in scrittura*/
   int write_lock;

   /*Numero di blocchi che compongono il device*/
   int blocksNumber;

   /*Numero di blocchi invalidati*/
   int invalidBlocksNumber;
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

void inizialize_meta_block(void); 
void set_block_device_onMount(char* devname);
void set_block_device_onUmount(void);
void increment_dim_file(int write_bytes);
void decrement_dim_file(int bytes);
struct block_device * get_block_device_AfterMount(void);
struct meta_block_rcu* read_ram_metablk(void); 
struct meta_block_rcu* read_device_metablk(void); 
void flush_device_metablk(void); 

#endif