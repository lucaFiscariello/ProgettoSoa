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
        meta_block_rcu->nextFreeBlock++;
        nextFreeBlock = meta_block_rcu->nextFreeBlock;
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


/**
 * Tale funzione si occupa di leggere un blocco dal device all'offset "block_to_read" e memorizzarlo nella struttura "block".
 * La lettura sfrutta API Linux per implementare rcu. In particolare:
 *  - rcu_read_lock : è un lock per i soli scrittori. I lettori possono concorrere senza problemi
 *  - rcu_dereference : è una macro che implementa un'assegnazione atomica attraverso ottimizzazioni al compilatore per l'accesso in memoria
 *  - rcu_read_unlock : libera eventuali scrittori in attesa.
*/
int read( int block_to_read,struct block* block){
    struct meta_block_rcu *meta_block_rcu;
    struct buffer_head *bh = NULL;
    struct block_device *block_device;
    void* temp;
    int ret;

    meta_block_rcu= read_ram_metablk();
    block_device = get_block_device_AfterMount();
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, block_to_read);

    check_block_index(block_to_read,meta_block_rcu);
    check_buffer_head(bh);

    if (bh->b_data != NULL){ 

        rcu_read_lock();
        temp = (void*) rcu_dereference(bh->b_data);
        memcpy( block,temp, DIM_BLOCK);
        rcu_read_unlock(); 

    }

#if SYN_PUT_DATA
    sync_dirty_buffer(bh);
#endif

    brelse(bh);

    return strlen(block->data);

}

/**
 * Questa funzione legge tutti i blocchi validi mantenuti nel device. La lettura segue l'ordine di delivery. Tutti i blocchi validi sono collegati tra di loro 
 * attraverso una serie di informazioni mantenute dai metadati del blocco stesso. In particolare ogni blocco nei propri metadati mantiene
 * gli ID dei blocchi scritti temporalmente prima e dopo di lui. In questo modo è implementata a tutti gli effetti una lista doppiamente collegata.
 * La resposabilità della funzione è quindi quella di individuare l'ID del primo blocco valido e scorrere la lista 
 * per leggere tutti i blocchi validi. L'ID del primo blocco valido è mantenuto in un campo del meta blocco.
 * La lettura è implementata utilizzando le API Linux rcu, pertanto un lettore leggerà tutti i blocchi ancora validi alla chiamata di 
 * "rcu_read_lock" anche se è presente uno scrittore concorrente.
*/
int read_all_block(char* data){

    struct buffer_head *bh = NULL;
    struct block *temp_block;
    struct meta_block_rcu *meta_block_rcu;
    struct block_device *block_device;
    int temp_block_to_read;
    void* temp;

    block_device = get_block_device_AfterMount();
    meta_block_rcu= read_ram_metablk();
    temp_block_to_read = meta_block_rcu->firstBlock;
    temp_block = kmalloc(DIM_BLOCK, GFP_KERNEL);

    bh = (struct buffer_head *)sb_bread(block_device->bd_super, temp_block_to_read);
    check_buffer_head(bh);

    //Segnalo presenza di un lettore a eventuali scrittori
    rcu_read_lock();
    
    // Leggo primo blocco valido
    temp = (void*) rcu_dereference(bh->b_data);

    if (temp != NULL){ 

        // Concateno tutti i dati letti in un unico buffer
        temp_block = kmalloc(DIM_BLOCK, GFP_KERNEL);
        memcpy( temp_block,temp, DIM_BLOCK);
        concat_data(data,temp_block);
        brelse(bh);

        // Scorro la lista dei blocchi validi seguendo l'ordine di consegna
        while (temp_block->next_block != BLOCK_ERROR){

            temp_block_to_read = temp_block->next_block;

            bh = (struct buffer_head *)sb_bread(block_device->bd_super, temp_block_to_read);
            check_bh_and_unlock(bh);

            temp = (void*) rcu_dereference(bh->b_data);

            if (temp != NULL){ 

                // Concateno tutti i dati letti in un unico buffer
                memcpy( temp_block,temp, DIM_BLOCK);
                concat_data(data,temp_block);
            }
            
            brelse(bh);
            
        }

    }else{
        brelse(bh);
    }

    rcu_read_unlock(); 

    return strlen(data);

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

    struct block_device *block_device = get_block_device_AfterMount();
    struct meta_block_rcu *meta_block_rcu = read_ram_metablk();;

    check_block_index(block_to_write,meta_block_rcu);

    struct buffer_head *bh = NULL;
    void* temp;
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, block_to_write);
      
    if (bh->b_data != NULL){

        // Memorizzio il riferimento ai dati in una variabile temporanea. In questo modo alla fine del greece period è possibile deallocarla
        temp = bh->b_data;
        rcu_assign_pointer(bh->b_data,(void *) block);
        synchronize_rcu();
        kfree(temp);

    }

    brelse(bh);
    
    return block_to_write;

}




