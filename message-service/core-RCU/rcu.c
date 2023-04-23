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
        return BLOCK_ERROR;
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
    printk("scrittura completata");
    return block_to_write;
}

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
    
    lock(meta_block_rcu->write_lock);

    check_return_read(read(block_to_invalidate,block));
    check_block_validity(block);

    block_update_pred = block->pred_block;
    block_update_next = block->next_block;

    check_return_read(read(block_update_pred,pred_block));
    check_return_read(read(block_update_next,next_block));

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

    return 0;

}

