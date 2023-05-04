/**
 * In questo file sono implementate le funzioni che permettono di :
 *  - leggere un blocco dal device
 *  - leggere tutti i blocchi validi dal device
 *  - scrivere un blocco sul device
 *  - invalidare un blocco sul device 
 * Il livello di astrazione dell'implementazione è tale per cui le funzioni vedono come input blocchi "vuoti" che devono essere
 * compilati con tutti i metadati e i dati necessari. In sostanza è responsabilità di queste funzioni gestire i metadati dei blocchi
 * affichè le future operazioni di scrittura/lettura/inavlidazione vadano a buon fine.
 * L'implementazione delle funzioni utilizza api di più basso livello implementate nel file "block_read_write.c".
 * 
*/

#include "lib/include/rcu.h"
#include "lib/include/block_read_write.h"
#include <linux/module.h>
#include <linux/slab.h>

int read_block_rcu(int block_to_read, struct block *block)
{
   return read(block_to_read, block);

}

int read_all_block_rcu(char *block_data)
{
    return read_all_block(block_data);
}

/**
 * Questa funzione implementa la scrittura di un blocco all'interno del device. Le sue responsabilità sono:
 *  - compilare i metadati del blocco affichè venga costruita correttamente la lista doppiamente collegata dei blocchi validi
 *  - prendere il lock in scrittura
 *  - chiamare le api di più basso livello per la scrittura effettiva del blocco che sfrutta rcu
*/
int write_rcu(char *block_data)
{

    struct meta_block_rcu *meta_block_rcu;
    struct block *block;
    struct block *pred_block;
    int block_to_write;
    int block_to_update;

    meta_block_rcu = read_ram_metablk();
    block_to_write = get_next_free_block();

    block = kmalloc(DIM_BLOCK, GFP_KERNEL);
    pred_block = kmalloc(DIM_BLOCK, GFP_KERNEL);

    check_block_index(block_to_write,meta_block_rcu);
   
    block_to_update = meta_block_rcu->lastWriteBlock;
    read(block_to_write, block);
    read(block_to_update, pred_block);

    //compilo metadati del blocco
    block->validity = VALID_BLOCK;
    block->next_block = BLOCK_ERROR;
    block->pred_block = block_to_update;
    pred_block->next_block = block_to_write;
    meta_block_rcu->lastWriteBlock = block_to_write;
    strncpy(block->data, block_data, DIM_DATA_BLOCK);

    //acquisisco lock in scrittura
    lock(meta_block_rcu->write_lock);

    //chiamo api basso livello per scrivere nuovo blocco
    write(block_to_write, block);

    //chiamo api basso livello per aggiornare blocco scritto temporalmente prima e implementare lista doppiamente collegata
    write(block_to_update, pred_block);

    unlock(meta_block_rcu->write_lock);

    return block_to_write;
}

/**
 * Questa funzione permette di invalidare e cancellare logicamente un blocco dal device. Le sue responsabilità sono:
 *  - prendere lock in scrittura
 *  - leggere il blocco da invalidare e i blocchi scritti temporalmente prima e dopo di esso
 *  - invalidare il blocco
 *  - collegare il blocco precedente con il blocco successivo cancellando logicamente il blocco invalidato dalla lista doppiamente collegata
 *  - aggiungere nella lista dei nodi invalidati il nodo appena invalidato
 *  - tutte le modifiche dei metadati effettuate sui 3 blocchi vengono riportate sul device
 * Si noti che tutti i blocchi invalidati vengono memorizzati in una linked list. In questo modo ,se necessario, alla successive
 * scritture verrà consultata questa lista per trovare un blocco da riutilizzare.
*/
int invalidate_rcu(int block_to_invalidate){

    struct meta_block_rcu *meta_block_rcu;
    struct invalid_block * new_invalid_block;
    struct block *block;
    struct block *pred_block;
    struct block *next_block;
    int block_update_pred;
    int block_update_next;

    meta_block_rcu = read_ram_metablk();
    block      = kmalloc(DIM_BLOCK, GFP_KERNEL);
    pred_block = kmalloc(DIM_BLOCK, GFP_KERNEL);
    next_block = kmalloc(DIM_BLOCK, GFP_KERNEL);
    new_invalid_block = kmalloc(sizeof(struct invalid_block),GFP_KERNEL);
    
    // acquisisco lock in scrittura
    lock(meta_block_rcu->write_lock);

    check_return_read(read(block_to_invalidate,block));
    check_block_validity(block);

    //individuo i blocchi scritti prima e dopo il blocco da invalidare
    block_update_pred = block->pred_block;
    block_update_next = block->next_block;

    check_return_read(read(block_update_pred,pred_block));
    check_return_read(read(block_update_next,next_block));

    block->validity = INVALID_BLOCK;

    //aggiorno i metadati dei blocchi in modo da cancellare logicamente nella lista doppiamente collegata di blocchi validi il blocco invalidato
    update_pointer_onInvalidate(pred_block,block_update_next,next_block,block_update_pred,meta_block_rcu);

    //creo nuovo nodo della linked list contenente il blocco invalidato
    new_invalid_block->block = block_to_invalidate;
    new_invalid_block->next = meta_block_rcu->headInvalidBlock;
    meta_block_rcu->headInvalidBlock = new_invalid_block;
    meta_block_rcu->invalidBlocksNumber++;

    // aggiorno tutti metadati dei 3 blocchi toccati
    write(block_to_invalidate, block);
    write(block_update_next,next_block);
    write(block_update_pred,pred_block);

    unlock(meta_block_rcu->write_lock);

    return 0;

}

