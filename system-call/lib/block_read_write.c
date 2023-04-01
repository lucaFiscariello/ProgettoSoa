#include <linux/kernel.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/string.h>
#include "include/block_read_write.h"

void read(char* devname,int block_to_read){

    struct block_device *block_device;
    struct buffer_head *bh = NULL;

    char destination[30];
    int size = 10;

    block_device= blkdev_get_by_path(devname, FMODE_READ|FMODE_WRITE, NULL);
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, block_to_read);
      
    if (bh->b_data != NULL){
        strncpy(destination, bh->b_data, size);  
        printk("Lettura blocco: %s ", destination);
    }

    brelse(bh);

}

void write(char* devname, int block_to_write){

    struct block_device *block_device;
    struct buffer_head *bh = NULL;

    char* destination = "proviamo e vediamo che succede";
    int size = 10;

    block_device= blkdev_get_by_path(devname, FMODE_READ|FMODE_WRITE, NULL);
    bh = (struct buffer_head *)sb_bread(block_device->bd_super, block_to_write);
      
    if (bh->b_data != NULL){
        strncpy(bh->b_data,destination, size);  
        printk("scrittura blocco: %s ", destination);
    }

    brelse(bh);

}