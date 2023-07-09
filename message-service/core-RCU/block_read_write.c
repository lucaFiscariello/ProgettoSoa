/**
 * In questo file sono implementate le funzioni che permettono di leggere e scrivere un blocco su un device.
 * Il livello di astrazione è tale per cui le funzioni si aspettano di scrivere e leggere blocchi "pronti". Non è responsabilità 
 * di queste funzioni creare il blocco e popolarlo con dati e metadati. Si assume che il blocco sia stato già arricchito dalle informazioni
 * necessarie. In particolare un blocco del device è modellato tramite la struttura "block" che prevede una serie di campi 
 * per mantenere metadati e dati utili.
*/

#include <linux/kernel.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/string.h>
#include "lib/include/block_read_write.h"


/**
 * Tale funzione si occupa di leggere un blocco dal device all'offset "block_to_read" e memorizzarlo nella struttura "block".
 * La lettura sfrutta API Linux per implementare rcu. In particolare:
 *  - rcu_read_lock : è un lock per i soli scrittori. I lettori possono concorrere senza problemi
 *  - rcu_dereference : è una macro che implementa un'assegnazione atomica attraverso ottimizzazioni al compilatore per l'accesso in memoria
 *  - rcu_read_unlock : libera eventuali scrittori in attesa.
*/
int read( int block_to_read,struct block* block){

    void* temp;
    
    struct meta_block_rcu *meta_block_rcu;
    struct block_device *block_device;
    struct buffer_head *bh = NULL;
    int epoch;

  
    meta_block_rcu= read_ram_metablk();
    block_device = get_block_device_AfterMount();
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, block_to_read);

    check_block_index(block_to_read,meta_block_rcu);
    check_buffer_head(bh);

    if (bh->b_data != NULL){ 

        epoch = rcu_lock_read();
        temp = (void*) bh->b_data;
        asm volatile("mfence");
        memcpy( block,temp, DIM_BLOCK);
        rcu_unlock_read(epoch); 

    }


    brelse(bh);

    return strlen(block->data);

}


/**
 * Questa funzione implementa la scrittura di un blocco nel device. Sfrutta API Linux rcu.
 * In particolare le api Linux utilizzate sono:
 *  - rcu_assign_pointer :  è una macro che implementa un'assegnazione atomica attraverso ottimizzazioni al compilatore per l'accesso in memoria
 *  - synchronize_rcu: attende la fine del greece period
 * Terminata l'attesa del greece period è possibile eliminare il buffer precedente in quanto nessun lettore lo sta più utilizzando.
 * 
 */
int write(int block_to_write,struct block* block){

    void* temp;
    struct block_device *block_device = get_block_device_AfterMount();
    struct meta_block_rcu *meta_block_rcu = read_ram_metablk();
    struct buffer_head *bh = NULL;
    
    check_block_index(block_to_write,meta_block_rcu);
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, block_to_write);
    check_buffer_head(bh);
    
    if (bh->b_data != NULL){

        update_epoch();
        temp = bh->b_data;
        bh->b_data=(void *) block;
        asm volatile("mfence");
        rcu_synchronize();
        kfree(temp);

    }
    
    #if SYN_PUT_DATA
    sync_dirty_buffer(bh);
    #endif

    brelse(bh);
    
    return block_to_write;

}

/**
 * Tale funzione è usata nella scrittura di un blocco. Il suo compito è quello di trovare un blocco libero in cui poter scrivere nuove 
 * informazioni. Come prima cosa il metodo  cerca il prossimo blocco libero all'interno del campo "nextFreeBlock" del metablocco. Se tale
 * campo contiene un valore non valido allora si cercherà un nuovo blocco nella lista dei blocchi precedentemente invalidati e mantenuta 
 * sempre nel metablocco. 
 * La lista contiene SOLO blocchi non validi per cui è sufficiente prelevare l'elemento in testa alla lista. Questo rende la ricerca del prossimo
 * blocco in cui scrivere costante e non lineare.
 * Se non sono presenti blocchi liberi è restituito il valore BLOCK_ERROR.
*/
int get_next_free_block(){

    int nextFreeBlock = BLOCK_ERROR;
    struct meta_block_rcu *meta_block_rcu;
    struct block_device *block_device;

    meta_block_rcu= read_ram_metablk();
    block_device = get_block_device_AfterMount();

    // Controllo il campo nextFreeBlock del metablocco
    if(meta_block_rcu->nextFreeBlock+1 < meta_block_rcu->blocksNumber){
        nextFreeBlock = meta_block_rcu->nextFreeBlock+1;
    }

    // Prelevo il primo elemento della lista dei blocchi non validi
    else{
        nextFreeBlock = meta_block_rcu->headInvalidBlock->block;

        //Verifico se il primo elemento della lista è valido
        if(meta_block_rcu->headInvalidBlock->next != NULL){
            meta_block_rcu->headInvalidBlock = meta_block_rcu->headInvalidBlock->next;
            meta_block_rcu->invalidBlocksNumber--;
        }
        else
            meta_block_rcu->headInvalidBlock->block = BLOCK_ERROR;
    }

    return nextFreeBlock;

}




