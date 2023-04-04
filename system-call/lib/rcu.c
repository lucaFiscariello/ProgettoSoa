#include "include/rcu.h"
#include "include/block_read_write.h"
#include <linux/module.h>
#include <linux/slab.h>

void read_block_rcu(int block_to_read,struct block* block){
    struct meta_block_rcu* meta_block_rcu;
    int my_epoch;

    meta_block_rcu = read_ram_metablk();
    my_epoch = meta_block_rcu->epoch;

    increment_counter(meta_block_rcu->arrayEpochCounter[my_epoch]);
    read(block_to_read,block);
    decrement_counter(meta_block_rcu->arrayEpochCounter[my_epoch]);

}

void read_all_block_rcu(char* block_data){
    struct meta_block_rcu* meta_block_rcu;
    struct block* temp_block;
    int temp_block_to_read;
    int my_epoch;

    meta_block_rcu = read_ram_metablk();
    my_epoch = meta_block_rcu->epoch;
    temp_block_to_read = meta_block_rcu->firstBlock;

    temp_block = kmalloc(DIM_BLOCK,GFP_KERNEL);

    increment_counter(meta_block_rcu->arrayEpochCounter[my_epoch]);

    read(temp_block_to_read,temp_block);
    printk("messaggio: %s",&temp_block->data );

    while(temp_block->next_block != BLOCK_ERROR){
        temp_block_to_read = temp_block->next_block;
        read(temp_block_to_read,temp_block);
        printk("messaggio: %s",&temp_block->data );

    }

    decrement_counter(meta_block_rcu->arrayEpochCounter[my_epoch]);



}

/* TODO: 
    testare il get_free_block
    la lettura non deve avvenire sui nodi non validi. Solo lo scrittore può farla.
    modifica i return delle funzioni
    togliere strcopy
    rendere lock un while
*/
int write_rcu(char* block_data){
    struct meta_block_rcu* meta_block_rcu;
    struct block *block;
    struct block *pred_block;
    int block_to_write;
    int block_to_update;
    int grace_epoch;
    int new_epoch;
    int my_epoch;

    meta_block_rcu = read_ram_metablk();
    block_to_write = get_next_free_block();
    block = kmalloc(DIM_BLOCK,GFP_KERNEL);
    pred_block = kmalloc(DIM_BLOCK,GFP_KERNEL);

    if(block_to_write == BLOCK_ERROR){
        printk("Nessun blocco disponibile!");
        return -1;
    }

    my_epoch = meta_block_rcu->epoch;
    block_to_update = meta_block_rcu->lastWriteBlock;

    increment_counter(meta_block_rcu->arrayEpochCounter[my_epoch]);
    read(block_to_write,block);
    read_only_metadata(block_to_update,pred_block);
    decrement_counter(meta_block_rcu->arrayEpochCounter[my_epoch]);

    lock(meta_block_rcu->write_lock);

    grace_epoch = new_epoch = meta_block_rcu->epoch;
	new_epoch +=1;
	new_epoch = new_epoch%ERAS;

    while(meta_block_rcu->arrayEpochCounter[new_epoch] > 0);

    block->validity= VALID_BLOCK;
    block->next_block = BLOCK_ERROR;
    block->pred_block = block_to_update;
    pred_block->next_block = block_to_write;
    meta_block_rcu->lastWriteBlock = block_to_write;

    strncpy(block->data,block_data,DIM_DATA_BLOCK);
    write(block_to_write,block);
    write_only_metadata(block_to_update,pred_block);

    meta_block_rcu->epoch = new_epoch;
	asm volatile("mfence");

    while(meta_block_rcu->arrayEpochCounter[grace_epoch] > 0);

    unlock(meta_block_rcu->write_lock);

    return block_to_write;
}
