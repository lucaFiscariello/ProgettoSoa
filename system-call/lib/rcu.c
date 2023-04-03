#include "include/rcu.h"
#include "include/block_read_write.h"
#include <linux/module.h>

void read_block_rcu(int block_to_read,struct block* block){
    struct meta_block_rcu* meta_block_rcu;
    int my_epoch;

    meta_block_rcu = read_ram_metablk();
    my_epoch = meta_block_rcu->epoch;

	__sync_fetch_and_add(&meta_block_rcu->arrayEpochCounter[my_epoch],1);

    read(block_to_read,block);

    __sync_fetch_and_add(&meta_block_rcu->arrayEpochCounter[my_epoch],-1);

}

/* TODO: 
    completare scrittura tenendo conto del greece period. 
    Gestire opportunamente nextFreeBlock. 
    Inserire collegamenti al blocco precedente e successivo.
    Scrivere che il blocco è valido.
*/
int write_rcu(struct block* block){
    struct meta_block_rcu* meta_block_rcu;
    int block_to_write;

    meta_block_rcu = read_ram_metablk();
    block_to_write = meta_block_rcu->nextFreeBlock;

    // Aggiorno il prossimo elemento libero
    if(block_to_write+1 >= meta_block_rcu->blockNumber)
        meta_block_rcu->nextFreeBlock++;
    else{
        meta_block_rcu->nextFreeBlock= meta_block_rcu->invalidBlock[0] ;
    }

    if(block_to_write == -1){
        printk("Nessun blocco disponibile!");
        return -1;
    }

    lock(meta_block_rcu->write_lock);
    write(block_to_write,block);
    unlock(meta_block_rcu->write_lock);

    return block_to_write;
}
