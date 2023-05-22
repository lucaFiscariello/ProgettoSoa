/**
 * Questo file contiene l'implementazioni di funzioni che permettono di accedere al "meta blocco". 
 * In questo progetto il meta blocco è un blocco di dati memorizzato nel device è che mantiene una serie di 
 * informazioni utili per getire la scrittura e la lettura concorrente di altri blocchi nel device. 
 * In particolare le funzioni implementate sono quelle di: 
 *  - inizializzazione del metablocco
 *  - flush del meta blocco dalla ram al device
 *  - creazione copia del meta in ram
*/

#include <linux/kernel.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/string.h>
#include <linux/kernel.h>

#include "lib/include/meta_block.h"

static struct block_device *block_device = NULL;
static struct meta_block_rcu *meta_block_rcu = NULL;
static int blocks_number = 0x0;

module_param(blocks_number, int, 0660);


/**
 *  L'inizializzazione del meta blocco consiste nel compilare i campi della struttura "meta_block_rcu" con valori di default 
 *  con conseguente scrittura della struttura all'interno di un blocco del device.
 *  Tale funzione deve essere invocata nel momento in cui un device è montato per la prima volta in assoluto, in modo da 
 *  formattare correttamente la struttura "meta_block_rcu" e garantire il corretto funzionamento delle API che dipendono da tale blocco.
*/
int inizialize_meta_block(){
    struct invalid_block* head;

    meta_block_rcu = kmalloc(sizeof(struct meta_block_rcu),GFP_KERNEL);
    meta_block_rcu = read_device_metablk();

    //Verifico se il metablocco è stato già inizializzato
    if(meta_block_rcu->already_inizialize)
        return 0;

    if(blocks_number>MAX_BLOCK)
        return -1;
        
    head = kmalloc(sizeof(struct invalid_block),GFP_KERNEL);
    head->block = BLOCK_ERROR;
    head->next = NULL;

    meta_block_rcu->lastWriteBlock = POS_META_BLOCK;
    meta_block_rcu->nextFreeBlock = POS_META_BLOCK;
    meta_block_rcu->firstBlock = POS_META_BLOCK+1;
    meta_block_rcu->already_inizialize = INIZIALIZE_META_BLK;
    meta_block_rcu->write_lock = ZERO_WRITER;

    meta_block_rcu->blocksNumber = blocks_number;
    meta_block_rcu->invalidBlocksNumber = 0;
    meta_block_rcu->headInvalidBlock = head;

               
    return 0;
}

/**
 * Il meta blocco è salvato in un blocco del device, tuttavia è mantenuta una copia anche in ram.
 * Ne consegue che se il meta blocco è soggetto a modifiche è necessario effettuare il flush di tale modifiche anche nel device. 
 * Tale funzione ha proprio questa responsabilità.
*/
int flush_device_metablk(){

    struct buffer_head *bh = NULL;

    bh = (struct buffer_head *)sb_bread(block_device->bd_super, POS_META_BLOCK);
    check_buffer_head(bh);

    if (bh->b_data != NULL){

        memcpy( bh->b_data,meta_block_rcu, sizeof(struct meta_block_rcu));    
        kfree(meta_block_rcu->headInvalidBlock);
            
    }

    brelse(bh);
    return 0;
}

/**
 * Dualmente al flush del meta blocco nel device , questa funzione permette recuperare il riferimento del metablocco tramite buffer head e salvare 
 * una copia in ram. I campi del meta blocco sono accessibili sia in scrittura che in lettura. Eventuali modifiche devono essere opportunamente
 * riportate sul device attraverso la funzione "flush_device_metablk".
*/
struct meta_block_rcu* read_device_metablk(){

    struct buffer_head *bh = NULL;
    struct invalid_block * new_invalid_block;

    bh = (struct buffer_head *)sb_bread(block_device->bd_super, POS_META_BLOCK);
    if(!bh) return NULL;


    if (bh->b_data != NULL){ 

        memcpy(meta_block_rcu, bh->b_data, sizeof(struct meta_block_rcu));        

        //Scorro tutta la bitmap mantenuta dal metablocco
        for(int blok_id=0 ; blok_id<meta_block_rcu->blocksNumber; blok_id++){

            //Se trovo un blocco invalidato creo un nuovo nodo nella linked list dei blocchi attualmente invalidati
            if(is_invalid(blok_id,meta_block_rcu)){

                new_invalid_block = kmalloc(sizeof(struct invalid_block),GFP_KERNEL);
                new_invalid_block->block = blok_id;
                new_invalid_block->next = meta_block_rcu->headInvalidBlock;
                meta_block_rcu->headInvalidBlock = new_invalid_block;

            }
         
        }

    }

    brelse(bh);
    return meta_block_rcu;
}


struct meta_block_rcu* read_ram_metablk(){
    if(meta_block_rcu==NULL)
        return read_device_metablk();

    return meta_block_rcu;
}

/**
 * In seguito ad un operazione di scritture è necessario incrementare il campo nell'inode che contiene la dimensione del file.
 * Questo metodo ha proprio questa responsabilità.
*/
int increment_dim_file(int write_bytes){

    struct buffer_head *bh = NULL;
    struct onefilefs_inode *inode;
    void** temp;

    bh = (struct buffer_head *)sb_bread(block_device->bd_super, POS_I_NODE);
    check_buffer_head(bh);

    if (bh->b_data != NULL){ 
        
        temp = (void**)bh->b_data;
        inode = (struct onefilefs_inode *)temp;
        inode->file_size = inode->file_size+write_bytes;

    }

    brelse(bh);
    return 0;
     
}

/**
 * In seguito ad un operazione di invalidazione di un blocco è necessario incrementare il campo nell'inode che contiene la dimensione del file.
 * Questo metodo ha proprio questa responsabilità.
*/
int decrement_dim_file(int bytes){

    struct buffer_head *bh = NULL;
    struct onefilefs_inode *inode;
    void** temp;

    bh = (struct buffer_head *)sb_bread(block_device->bd_super, POS_I_NODE);
    check_buffer_head(bh);

    if (bh->b_data != NULL){ 
        
        temp = (void**)bh->b_data;
        inode = (struct onefilefs_inode *)temp;
        inode->file_size = inode->file_size-bytes;

    }

    brelse(bh);
    return 0;
}


void set_block_device_onMount(const char* devname){
    block_device= blkdev_get_by_path(devname, FMODE_READ|FMODE_WRITE, NULL);
}

void set_block_device_onUmount(){
    block_device= NULL;
}

struct block_device * get_block_device_AfterMount(void){
    return block_device;
}


