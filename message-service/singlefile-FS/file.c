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
#include <linux/version.h>	

#include "lib/include/singlefilefs.h"
#include "../core-RCU/lib/include/rcu.h"

#define COMPLETE 1
#define NO_COMPLETE 0

/**
 * La funzione di lettura scorre tutti i blocchi validi in ordine di consegna.
 * I blocchi sono organizzati logicamente in una linked list doppiamente collegata, pertanto uno scrittore dovrà 
 * scorrere la lista e copiare il contenuto dei blochi nel buffer utente. La funzione è implementata nel seguente modo:
 *  - vengono notificati eventuali scrittori che un lettore ha avviato un operazione di lettura
 *  - si individua il blocco da cui iniziare le operazioni di lettura.
 *  - si scorrono tutti i blocchi e si memorizzano i dati nel buffer utente un blocco alla volta. Vengono copiati solo i dati utili.
*/
ssize_t onefilefs_read(struct file * filp, char __user * buf, size_t len, loff_t * off) {
     
    int ret;
    int temp_block_to_read;
    void* temp;
    struct block *temp_block;
    struct meta_block_rcu *meta_block_rcu;
    struct block_device *block_device;
    struct buffer_head *bh = NULL;
    struct private_info* private_info;
  

    meta_block_rcu = read_ram_metablk();
    check_mount(meta_block_rcu);

    block_device = get_block_device_AfterMount();
    private_info = (struct private_info*)filp->private_data;

    //Alla prima invocazione filp->private_data sarà null
    if(filp->private_data == NULL){

        filp->private_data = kmalloc(sizeof(struct private_info),GFP_KERNEL);
        private_info = (struct private_info*)filp->private_data;

        //mantengo alcune informazioni di sessione
        private_info->current_block= meta_block_rcu->firstBlock;
        private_info->epoch = rcu_lock_read();
        private_info->complete = NO_COMPLETE; 

    }

    //Alle successive invocazioni verifico se ho letto tutti i dati a disposizione
    if( private_info->complete == COMPLETE ){
        return 0;
    }


    /* 
     * private_info->current_block indica il blocco da cui partire la lettura. Può ossere il primo blocco valido del device
     * oppure un qualsiasi altro blocco se la funzione è invocata più di una volta.
     */
    temp_block_to_read = private_info->current_block;

    //Scorro la lista di tutti i blocchi validi fin quando o non li leggo tutti o ho terminato lo spazio nel buffer utente
    while (temp_block_to_read != BLOCK_ERROR && *off<len){

        if(temp_block_to_read<=POS_META_BLOCK){
            rcu_unlock_read(private_info->epoch);
            return 0;
        }

        bh = (struct buffer_head *)sb_bread(block_device->bd_super, temp_block_to_read);
        check_bh_and_unlock(bh,private_info->epoch);

        //acquisisco riferimento del blocco temporaneo
        temp = (void*) (bh->b_data);
        temp_block = (struct block*) (void**)temp;

        if (temp != NULL){ 

            //Copio i dati nel buffer utente un blocco alla volta
            ret = copy_to_user(buf+*off,temp_block->data, DIM_DATA_BLOCK);
            strcat(buf+*off+1,"\n");
            *off+=strlen(temp_block->data)-ret+1;

            //Mi sposto al blocco successivo
            temp_block_to_read = temp_block->next_block;
            brelse(bh);

        }else{

            rcu_unlock_read(private_info->epoch);
            brelse(bh);
            return -1;

        }
    
    }

    private_info->current_block=temp_block_to_read;

    //Se ho completato la lettura lo memorizzo nei dati privati
    if( private_info->current_block <= POS_META_BLOCK || *off>=len ){
        rcu_unlock_read(private_info->epoch);
        private_info->complete= COMPLETE;
    }

    return *off;
        

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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
	    inode_init_owner(&init_user_ns,the_inode, NULL, S_IFREG);
#else
        inode_init_owner(the_inode, NULL, S_IFREG);
#endif


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
	
    check_mount(read_ram_metablk());

    increment_active_threads();
	try_module_get(THIS_MODULE);

    //non è possibile effettuare operazioni di scrittura
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY || (filp->f_flags & O_ACCMODE) == O_RDWR){
		return WRITE_ERROR;
	}

	return 0;
}

int onefilefs_release(struct inode *inode, struct file *filp){

    decrement_active_threads();
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
