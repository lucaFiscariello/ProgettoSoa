#include <linux/kernel.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/string.h>
#include "lib/include/block_read_write.h"


int get_next_free_block(){

    int nextFreeBlock = BLOCK_ERROR;
    struct meta_block_rcu *meta_block_rcu;
    struct block_device *block_device;

    meta_block_rcu= read_ram_metablk();
    block_device = get_block_device_AfterMount();

    if(meta_block_rcu->nextFreeBlock+1 < meta_block_rcu->blocksNumber){
        meta_block_rcu->nextFreeBlock++;
        nextFreeBlock = meta_block_rcu->nextFreeBlock;
    }
    else{
        nextFreeBlock = meta_block_rcu->headInvalidBlock->block;

        if(meta_block_rcu->headInvalidBlock->next != NULL)
            meta_block_rcu->headInvalidBlock = meta_block_rcu->headInvalidBlock->next;
        else
            meta_block_rcu->headInvalidBlock->block = BLOCK_ERROR;
    }

    return nextFreeBlock;

}


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
   
    if (bh->b_data != NULL){ 

        rcu_read_lock();
        temp = (void*) rcu_dereference(bh->b_data);
        memcpy( block,temp, DIM_BLOCK);
        rcu_read_unlock(); 

    }


    brelse(bh);

    check_block_validity(block);

    return strlen(block->data);

}

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

    rcu_read_lock();

    bh = (struct buffer_head *)sb_bread(block_device->bd_super, temp_block_to_read);
    temp = (void*) rcu_dereference(bh->b_data);

    if (temp != NULL){ 

        memcpy( temp_block,temp, DIM_BLOCK);
        concat_data(data,temp_block);
        brelse(bh);

        while (temp_block->next_block != BLOCK_ERROR){

            temp_block_to_read = temp_block->next_block;

            bh = (struct buffer_head *)sb_bread(block_device->bd_super, temp_block_to_read);
            temp = (void*) rcu_dereference(bh->b_data);


            if (temp != NULL){ 
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

int write(int block_to_write,struct block* block){

    struct block_device *block_device = get_block_device_AfterMount();
    struct meta_block_rcu *meta_block_rcu = read_ram_metablk();;

    check_block_index(block_to_write,meta_block_rcu);

    struct buffer_head *bh = NULL;
    void* temp;
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, block_to_write);
      
    if (bh->b_data != NULL){

        temp = bh->b_data;
        rcu_assign_pointer(bh->b_data,(void *) block);
        synchronize_rcu();
        kfree(temp);

    }

    brelse(bh);
    
    return block_to_write;

}




