#include "include/rcu.h"
#include "include/block_read_write.h"
#include <linux/module.h>
#include <linux/slab.h>

void read_block_rcu(int block_to_read, struct block *block)
{
    read(block_to_read, block);
}

void read_all_block_rcu(char *block_data)
{
    read_all_block(block_data);
}

/* TODO:
    testare il get_free_block
    la lettura non deve avvenire sui nodi non validi. Solo lo scrittore può farla.
    modifica i return delle funzioni
    togliere strcopy
    mettere controllo su while : all o niente
    // per invalidate controllo dimensione massima, modificare if in cascata
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

    if (block_to_write == BLOCK_ERROR)
    {
        printk("Nessun blocco disponibile!");
        return -1;
    }

    block_to_update = meta_block_rcu->lastWriteBlock;

    read(block_to_write, block);
    read(block_to_update, pred_block);

    lock(meta_block_rcu->write_lock);

    block->validity = VALID_BLOCK;
    block->next_block = BLOCK_ERROR;
    block->pred_block = block_to_update;
    pred_block->next_block = block_to_write;
    meta_block_rcu->lastWriteBlock = block_to_write;

    strncpy(block->data, block_data, DIM_DATA_BLOCK);
    write(block_to_write, block);
    write(block_to_update, pred_block);

    unlock(meta_block_rcu->write_lock);

    return block_to_write;
}

void invalidate_rcu(int block_to_invalidate){
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

    lock(meta_block_rcu->write_lock);

    read(block_to_invalidate,block);
    block_update_pred = block->pred_block;
    block_update_next = block->next_block;

    read(block_update_pred,pred_block);
    read(block_update_next,next_block);

    block->validity = INVALID_BLOCK;

    if(pred_block !=NULL)
        pred_block->next_block = block_update_next;
    
    if(next_block !=NULL)
        next_block->pred_block = block_update_pred;

    if(block_to_invalidate== meta_block_rcu->firstBlock){

        if(block_update_next == BLOCK_ERROR ){
            meta_block_rcu->firstBlock = POS_META_BLOCK+1;
        }else{
            meta_block_rcu->firstBlock = block_update_next;
        }

    }

    else if (block_to_invalidate == meta_block_rcu->lastWriteBlock){
      
        meta_block_rcu->lastWriteBlock = block_update_pred;

    }

    new_invalid_block->block = block_to_invalidate;
    meta_block_rcu->headInvalidBlock = new_invalid_block;

    write(block_to_invalidate, block);
    write(block_update_next,next_block);
    write(block_update_pred,pred_block);


    unlock(meta_block_rcu->write_lock);


}

