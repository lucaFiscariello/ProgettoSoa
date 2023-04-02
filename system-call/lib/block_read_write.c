#include <linux/kernel.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/string.h>
#include "include/block_read_write.h"

void read(char* devname, int block_to_read,struct block* block){

    struct block_device *block_device;
    struct buffer_head *bh = NULL;


    block_device= blkdev_get_by_path(devname, FMODE_READ|FMODE_WRITE, NULL);
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, block_to_read);
      
    if (bh->b_data != NULL){
        memcpy( block,bh->b_data, DIM_BLOCK);         
    }

    brelse(bh);

}

void write(char* devname, int block_to_write,struct block* block){

    struct block_device *block_device;
    struct buffer_head *bh = NULL;

    block_device= blkdev_get_by_path(devname, FMODE_READ|FMODE_WRITE, NULL);
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, block_to_write);
      
    if (bh->b_data != NULL){
        memcpy( bh->b_data,block, DIM_BLOCK);         
    }

    brelse(bh);

}

void read_meta_block(char* devname){
    struct block_device *block_device;
    struct buffer_head *bh = NULL;
    struct meta_block_rcu meta_block_rcu;

    block_device= blkdev_get_by_path(devname, FMODE_READ|FMODE_WRITE, NULL);
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, POS_META_BLOCK);
      
    if (bh->b_data != NULL){
        
        memcpy(&meta_block_rcu, bh->b_data, sizeof(struct meta_block_rcu));        
        printk("Lettura epoca: %d ", meta_block_rcu.epoch);
        printk("Lettura first: %d ", meta_block_rcu.firstBlock);

 
    }

    
    brelse(bh);
}

void write_meta_block(char* devname){
    struct block_device *block_device;
    struct buffer_head *bh = NULL;
    struct meta_block_rcu *meta_block_rcu;

    block_device= blkdev_get_by_path(devname, FMODE_READ|FMODE_WRITE, NULL);
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, POS_META_BLOCK);
      
    if (bh->b_data != NULL){
        
        meta_block_rcu = kmalloc(sizeof(struct meta_block_rcu),GFP_KERNEL);
        meta_block_rcu->epoch = 10;
        memcpy( bh->b_data,meta_block_rcu, sizeof(struct meta_block_rcu));        
        printk("Scrittura epoca: %d ", meta_block_rcu->epoch);

 
    }

    
    brelse(bh);
}
