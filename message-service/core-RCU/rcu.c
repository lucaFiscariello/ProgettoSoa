#include "lib/include/rcu.h"
#include "lib/include/block_read_write.h"
#include <linux/module.h>
#include <linux/slab.h>

void read_block_rcu(int block_to_read, struct block *block)
{
    read(block_to_read, block);

    if(block->validity == INVALID_BLOCK){
        block = NULL;
        return;
    }
}

void read_all_block_rcu(char *block_data)
{
    read_all_block(block_data);
}

/* TODO:
    modifica i return delle funzioni
    mettere controllo su while : all o niente
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

    if(block_to_invalidate<0 || block_to_invalidate>meta_block_rcu->blocksNumber)
        return;

    lock(meta_block_rcu->write_lock);

    read(block_to_invalidate,block);
    block_update_pred = block->pred_block;
    block_update_next = block->next_block;

    read(block_update_pred,pred_block);
    read(block_update_next,next_block);

    block->validity = INVALID_BLOCK;

    update_pointer_onInvalidate(pred_block,block_update_next,next_block,block_update_pred,meta_block_rcu);

    new_invalid_block->block = block_to_invalidate;
    new_invalid_block->next = meta_block_rcu->headInvalidBlock;
    meta_block_rcu->headInvalidBlock = new_invalid_block;
    meta_block_rcu->invalidBlocksNumber++;

    write(block_to_invalidate, block);
    write(block_update_next,next_block);
    write(block_update_pred,pred_block);


    unlock(meta_block_rcu->write_lock);


}

