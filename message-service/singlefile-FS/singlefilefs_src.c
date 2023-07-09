#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "lib/include/singlefilefs.h"
#include "../core-RCU/lib/include/rcu.h"


static int lock_mount=0;                                                          


static struct super_operations singlefilefs_super_ops = {
};


static struct dentry_operations singlefilefs_dentry_ops = {
};


int singlefilefs_fill_super(struct super_block *sb, void *data, int silent) {   

    struct inode *root_inode;
    struct buffer_head *bh;
    struct onefilefs_sb_info *sb_disk;
    struct timespec64 curr_time;
    uint64_t magic;


    //Unique identifier of the filesystem
    sb->s_magic = MAGIC;

    bh = sb_bread(sb, SB_BLOCK_NUMBER);
    if(!sb){
	return -EIO;
    }
    sb_disk = (struct onefilefs_sb_info *)bh->b_data;
    magic = sb_disk->magic;
    brelse(bh);

    //check on the expected magic number
    if(magic != sb->s_magic){
	return -EBADF;
    }

    sb->s_fs_info = NULL; //FS specific data (the magic number) already reported into the generic superblock
    sb->s_op = &singlefilefs_super_ops;//set our own operations


    root_inode = iget_locked(sb, 0);//get a root inode indexed with 0 from cache
    if (!root_inode){
        return -ENOMEM;
    }

    root_inode->i_ino = SINGLEFILEFS_ROOT_INODE_NUMBER;//this is actually 10
    inode_init_owner(&init_user_ns, root_inode, NULL, S_IFDIR);// set the root user as owned of the FS root
    root_inode->i_sb = sb;
    root_inode->i_op = &onefilefs_inode_ops;//set our inode operations
    root_inode->i_fop = &onefilefs_dir_operations;//set our file operations
    //update access permission
    root_inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;

    //baseline alignment of the FS timestamp to the current time
    ktime_get_real_ts64(&curr_time);
    root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = curr_time;

    // no inode from device is needed - the root of our file system is an in memory object
    root_inode->i_private = NULL;

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root)
        return -ENOMEM;

    sb->s_root->d_op = &singlefilefs_dentry_ops;//set our dentry operations

    //unlock the inode to make it usable
    unlock_new_inode(root_inode);


    return 0;
}

static void singlefilefs_kill_superblock(struct super_block *s) {
    
    struct meta_block_rcu *meta_block_rcu;
    meta_block_rcu = read_ram_metablk();

    // Non permetto a nuovi thread di iniziare operazioni di lettura/scritttura/invalidazione
    change_state_mount(meta_block_rcu);
    flush_device_metablk();

    //Attendo che i thread pendenti terminino le proprie operazioni
    wait_umount();

    set_block_device_onUmount();
    kill_block_super(s);
    unlock(lock_mount);

    printk(KERN_INFO "%s: singlefilefs unmount succesful.\n",MOD_NAME);
    return;
}

//called on file system mounting 
struct dentry *singlefilefs_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {

    struct dentry *ret;
    struct meta_block_rcu *meta_block_rcu;
    
    //un solo montaggio per volta può avvenire
    try_lock_mount(lock_mount);

    ret = mount_bdev(fs_type, flags, dev_name, data, singlefilefs_fill_super);

    if (unlikely(IS_ERR(ret)))
        printk("%s: error mounting onefilefs",MOD_NAME);
    else{
        
        set_block_device_onMount(dev_name);
        if(inizialize_meta_block()<0)
            return NULL;

        meta_block_rcu = read_ram_metablk();
        change_state_mount(meta_block_rcu);

        printk("%s: singlefilefs is succesfully mounted on from device %s. Mount: %d\n",MOD_NAME,dev_name,meta_block_rcu->mount_state);

    }




    return ret;
}

//file system structure
static struct file_system_type onefilefs_type = {
	.owner = THIS_MODULE,
        .name           = "singlefilefs",
        .mount          = singlefilefs_mount,
        .kill_sb        = singlefilefs_kill_superblock,
};


int singlefilefs_init(void) {

    int ret;

    //register filesystem
    ret = register_filesystem(&onefilefs_type);
    if (likely(ret == 0))  
        printk("%s: sucessfully registered singlefilefs\n",MOD_NAME);
    else
        printk("%s: failed to register singlefilefs - error %d", MOD_NAME,ret);


    return ret;
}

void singlefilefs_exit(void) {

    int ret;

    //unregister filesystem
    ret = unregister_filesystem(&onefilefs_type);

    if (likely(ret == 0))
       printk("%s: sucessfully unregistered file system driver\n",MOD_NAME);
    else
        printk("%s: failed to unregister singlefilefs driver - error %d", MOD_NAME, ret);
}


