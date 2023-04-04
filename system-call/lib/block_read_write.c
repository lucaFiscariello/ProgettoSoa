#include <linux/kernel.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/string.h>
#include "include/block_read_write.h"

static struct block_device *block_device = NULL;
static struct meta_block_rcu *meta_block_rcu = NULL;
static int blocks_number = 100;

void set_block_device_onMount(char* devname){
    block_device= blkdev_get_by_path(devname, FMODE_READ|FMODE_WRITE, NULL);
}

void set_block_device_onUmount(){
    block_device= NULL;
}

void inizialize_meta_block(){
    struct buffer_head *bh = NULL;
    struct invalid_block* head;

    bh = (struct buffer_head *)sb_bread(block_device->bd_super, POS_META_BLOCK);
      
    if (bh->b_data != NULL){

        head = kmalloc(sizeof(struct invalid_block),GFP_KERNEL);
        head->block = -1;
        head->next = NULL;

        meta_block_rcu = kmalloc(sizeof(struct meta_block_rcu),GFP_KERNEL);
        meta_block_rcu->lastWriteBlock = POS_META_BLOCK;
        meta_block_rcu->nextFreeBlock = POS_META_BLOCK;
        meta_block_rcu->blocksNumber = blocks_number;
        meta_block_rcu->blocksNumber = 0;
        meta_block_rcu->headInvalidBlock = head;

        memcpy( bh->b_data,meta_block_rcu, sizeof(struct meta_block_rcu));        
 
    }

    brelse(bh);
}

void flush_device_metablk(){

    struct buffer_head *bh = NULL;
    struct invalid_block* head = meta_block_rcu->headInvalidBlock;
    int pos = 0;

    bh = (struct buffer_head *)sb_bread(block_device->bd_super, POS_META_BLOCK);
      
    if (bh->b_data != NULL){

        while(head->next != NULL && head->block != -1 )
            meta_block_rcu->invalidBlocks[pos++] = head->block;

        memcpy( bh->b_data,meta_block_rcu, sizeof(struct meta_block_rcu));        
    }

    brelse(bh);
}

struct meta_block_rcu* read_ram_metablk(){
    if(meta_block_rcu==NULL)
        return read_device_metablk();

    return meta_block_rcu;
}

struct meta_block_rcu* read_device_metablk(){

    struct buffer_head *bh = NULL;
    struct invalid_block * new_invalid_block;

    bh = (struct buffer_head *)sb_bread(block_device->bd_super, POS_META_BLOCK);
      
    if (bh->b_data != NULL){ 

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

int get_next_free_block(){

    int nextFreeBlock = -1;

    if(meta_block_rcu->nextFreeBlock+1 < blocks_number){
        meta_block_rcu->nextFreeBlock++;
        nextFreeBlock = meta_block_rcu->nextFreeBlock;
    }
    else{
        nextFreeBlock = meta_block_rcu->headInvalidBlock->block;

        if(meta_block_rcu->headInvalidBlock->next != NULL)
            meta_block_rcu->headInvalidBlock = meta_block_rcu->headInvalidBlock->next;
        else
            meta_block_rcu->headInvalidBlock->block = -1;
    }

    return nextFreeBlock;

}


void read( int block_to_read,struct block* block){

    struct buffer_head *bh = NULL;
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, block_to_read);
      
    if (bh->b_data != NULL){  
        memcpy( block,bh->b_data, DIM_BLOCK);   
    }

    brelse(bh);

}

void write(int block_to_write,struct block* block){

    struct buffer_head *bh = NULL;
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, block_to_write);
      
    if (bh->b_data != NULL){
        memcpy( bh->b_data,block, DIM_BLOCK);         
    }

    brelse(bh);

}


