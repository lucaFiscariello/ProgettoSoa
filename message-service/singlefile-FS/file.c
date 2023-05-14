#include <linux/init.h>
#include <linux/fcntl.h>
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


/**
 * La funzione di lettura è implementata nel seguente modo:
 *  - si sfrutta l'api read_all_block_rcu() che legge tutti i blocchi validi seguendo l'ordine di delivery
 *  - tutti i blocchi letti vengono concatenati e memorizzati in un buffer temporaneo
 *  - il puntatore del buffer temporaneo è mantenuto in filp->private_data
 * Questa implementazione offre garanzie se durante la lettura avviene una scrittura concorrente. Terminata l'esecuzione di read_all_block_rcu() si avrà a disposizione
 * in un buffer locale dei dati che non possono più essere toccati da nessuno scrittore. Questo permette di rilasciare le informazioni che 
 * sono presenti nel buffer temporaneo un blocco alla volta di dimensione "len" senza che nessun altro thread interferisca.
 * I dati che sono mantenuti nel buffer temporaneo sono stati privati dei metadati.
*/
ssize_t onefilefs_read(struct file * filp, char __user * buf, size_t len, loff_t * off) {

    
    char* kernel_buffer;
    int ret;

    // il campo filp->private_data sarà NULL alla prima invocazione della funzione
    if(filp->private_data == NULL){

        //Alla prima invocazione della funzione leggo tutti i blocchi e li memorizzo in un buffer temporaneo
        kernel_buffer = kmalloc(len,GFP_KERNEL);
        read_all_block_rcu(kernel_buffer);
        filp->private_data = kernel_buffer;

    }else{

        //Verifico se ho letto tutti i dati dal buffer temporaneo
        if(*off >= strlen(filp->private_data)){

            kfree(filp->private_data);
            return 0;

        }
    }

    //Copio i dati nel buffer utente un blocco alla volta di dimensione "len"
    filp->private_data;
    ret = copy_to_user(buf,filp->private_data + *off , len);
    *off += len-ret; 

    return len-ret;

}


struct dentry *onefilefs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flags) {

    struct onefilefs_inode *FS_specific_inode;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh = NULL;
    struct inode *the_inode = NULL;

    printk("%s: running the lookup inode-function for name %s",MOD_NAME,child_dentry->d_name.name);

    if(!strcmp(child_dentry->d_name.name, UNIQUE_FILE_NAME)){

	
	//get a locked inode from the cache 
        the_inode = iget_locked(sb, 1);
        if (!the_inode)
       		 return ERR_PTR(-ENOMEM);

	//already cached inode - simply return successfully
	if(!(the_inode->i_state & I_NEW)){
		return child_dentry;
	}


	//this work is done if the inode was not already cached
    inode_init_owner(&init_user_ns, the_inode, NULL, S_IFREG);// set the root user as owned of the FS root
	the_inode->i_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IXUSR | S_IXGRP | S_IXOTH;
    the_inode->i_fop = &onefilefs_file_operations;
	the_inode->i_op = &onefilefs_inode_ops;

	//just one link for this file
	set_nlink(the_inode,1);

	//now we retrieve the file size via the FS specific inode, putting it into the generic inode
    bh = (struct buffer_head *)sb_bread(sb, SINGLEFILEFS_INODES_BLOCK_NUMBER );
    if(!bh){
		iput(the_inode);
		return ERR_PTR(-EIO);
    }

	FS_specific_inode = (struct onefilefs_inode*)bh->b_data;
	the_inode->i_size = FS_specific_inode->file_size;
    brelse(bh);

    d_add(child_dentry, the_inode);
	dget(child_dentry);

	//unlock the inode to make it usable 
    unlock_new_inode(the_inode);

	return child_dentry;
    }

    return NULL;

}

int onefilefs_open(struct inode *inode, struct file *filp){
	
    check_mount();

	// incremento usage count
	try_module_get(THIS_MODULE);

    //non è possibile effettuare operazioni di scrittura
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY || (filp->f_flags & O_ACCMODE) == O_RDWR){
		return WRITE_ERROR;
	}

	return 0;
}

int onefilefs_release(struct inode *inode, struct file *filp){

	check_mount();
	
	// decremento usage count
	module_put(THIS_MODULE);

	return 0;
}

//look up goes in the inode operations
const struct inode_operations onefilefs_inode_ops = {
    .lookup = onefilefs_lookup,
};

const struct file_operations onefilefs_file_operations = {
    .owner = THIS_MODULE,
    .read = onefilefs_read,
    .open = onefilefs_open,
	.release = onefilefs_release,
};
