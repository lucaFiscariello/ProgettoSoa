/**
 * Questo file contiene l'implementazioni di funzioni che permettono di accedere al "meta blocco". 
 * In questo progetto il meta blocco è un blocco di dati memorizzato nel device è che mantiene una serie di 
 * informazioni utili per getire la scrittura e la lettura concorrente di altri blocchi nel device. 
 * In particolare le funzioni implementate sono quelle di: 
 *  - inizializzazione del metablocco
 *  - flush del meta blocco dalla ram al device
 *  - lettura del meta blocco dal device alla ram
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
void inizialize_meta_block(){
    struct buffer_head *bh = NULL;
    struct invalid_block* head;

    bh = (struct buffer_head *)sb_bread(block_device->bd_super, POS_META_BLOCK);
      
    if (bh->b_data != NULL){

        head = kmalloc(sizeof(struct invalid_block),GFP_KERNEL);
        head->block = BLOCK_ERROR;
        head->next = NULL;

        meta_block_rcu = kmalloc(sizeof(struct meta_block_rcu),GFP_KERNEL);
        meta_block_rcu->lastWriteBlock = POS_META_BLOCK;
        meta_block_rcu->nextFreeBlock = POS_META_BLOCK;
        meta_block_rcu->firstBlock = POS_META_BLOCK+1;
        meta_block_rcu->blocksNumber = blocks_number;
        meta_block_rcu->invalidBlocksNumber = 0;
        meta_block_rcu->headInvalidBlock = head;

        memcpy( bh->b_data,meta_block_rcu, sizeof(struct meta_block_rcu));        
 
    }

    brelse(bh);
}

/**
 * Il meta blocco è salvato in un blocco del device, tuttavia per motivi di prestazione è mantenuta una copia del blocco anche in ram.
 * Ne consegue che se il meta blocco è soggetto a modifiche è necessario effettuare il flush di tale modifiche anche nel device. 
 * Tale funzione ha proprio questa responsabilità.
*/
void flush_device_metablk(){

    struct buffer_head *bh = NULL;
    struct invalid_block* head = meta_block_rcu->headInvalidBlock;
    int pos = 0;

    bh = (struct buffer_head *)sb_bread(block_device->bd_super, POS_META_BLOCK);
      
    if (bh->b_data != NULL){
        
        /**
         * Memorizzo nel meta blocco anche gli id dei blocchi invalidati. In ram tutti gli id dei blocchi sono mantenuti 
         * in una linked list. Nel meta blocco sono invece memorizzati in un array. Il ciclo while effetua la conversione
         * da linked list a array.
        */
        while(head->next != NULL && head->block != BLOCK_ERROR ){
            
            meta_block_rcu->invalidBlocks[pos++] = head->block;
            head = head->next;

        }

        memcpy( bh->b_data,meta_block_rcu, sizeof(struct meta_block_rcu));        
    }

    brelse(bh);
}

/**
 * Dualmente al flush del meta blocco nel device , questa funzione permette di leggere il metablocco dal device e salvare 
 * una copia in ram. I campi del meta blocco sono accessibili sia in scrittura che in lettura. Eventuali modifiche devono essere opportunamente
 * riportate sul device attraverso la funzione "flush_device_metablk".
*/
struct meta_block_rcu* read_device_metablk(){

    struct buffer_head *bh = NULL;
    struct invalid_block * new_invalid_block;

    bh = (struct buffer_head *)sb_bread(block_device->bd_super, POS_META_BLOCK);
      
    if (bh->b_data != NULL){ 

       /**
         * Nel meta blocco sono memorizzati anche gli id dei blocchi invalidati. In ram tutti gli id dei blocchi sono mantenuti 
         * in una linked list. Nel meta blocco sono invece memorizzati in un array. Il ciclo for effetua la conversione
         * da array a  linked list .
        */
        for(int i=0 ; i<meta_block_rcu->invalidBlocksNumber; i++){
            new_invalid_block = kmalloc(sizeof(struct invalid_block),GFP_KERNEL);
            new_invalid_block->block = meta_block_rcu->invalidBlocks[i];
            new_invalid_block->next = meta_block_rcu->headInvalidBlock;
            meta_block_rcu->headInvalidBlock = new_invalid_block;
        }

        memcpy(meta_block_rcu, bh->b_data, sizeof(struct meta_block_rcu));        
    }

    brelse(bh);
    return meta_block_rcu;
}

/**
 * Tale funzione permette di implementare una cache in ram. Quando un thread ha bisogno di leggere il meta blocco 
 * se è già presente in ram non c'è necessita di leggerlo nuovamente dal device.
*/
struct meta_block_rcu* read_ram_metablk(){
    if(meta_block_rcu==NULL)
        return read_device_metablk();

    return meta_block_rcu;
}

void set_block_device_onMount(char* devname){
    block_device= blkdev_get_by_path(devname, FMODE_READ|FMODE_WRITE, NULL);
}

void set_block_device_onUmount(){
    block_device= NULL;
}

struct block_device * get_block_device_AfterMount(void){
    return block_device;
}


