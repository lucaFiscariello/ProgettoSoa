#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/syscalls.h>

#include "lib/include/scth.h"
#include "lib/include/system-call.h"
#include "../core-RCU/lib/include/rcu.h"
#include "../singlefile-FS/lib/include/singlefilefs.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Luca Fiscariello");

#define MODNAME "Message_service"
#define NO_MAP (-EFAULT)
#define AUDIT if(1)

unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);

int PUT = 0;
module_param(PUT, int, 0660);

int GET = 0;
module_param(GET, int, 0660);

int INVALIDATE = 0;
module_param(INVALIDATE, int, 0660);

unsigned long the_ni_syscall;
unsigned long new_sys_call_array[] = {0x0,0x0,0x0};

#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};


__SYSCALL_DEFINEx(2, _put_data, char*, source,size_t, size){

        check_mount();

        int block_number;
        int ret;
        char* kernel_buffer = kmalloc(size,GFP_KERNEL);
        copy_from_user(kernel_buffer, source, size);
        
        ret = write_rcu(kernel_buffer);
	return  ret;
	
}

__SYSCALL_DEFINEx(3, _get_data, int, offset, char*, source,size_t, size){

        check_mount();

        int ret;
        struct block* block = kmalloc(DIM_BLOCK,GFP_KERNEL);
        
        ret = read_block_rcu(offset,block);
        copy_to_user(source, block->data, size);

	return ret;
	
}

__SYSCALL_DEFINEx(1, _invalidate_data, int, offset){

        check_mount();
	return invalidate_rcu(offset);
	
}

long sys_invalidate_data = (unsigned long) __x64_sys_invalidate_data;       
long sys_put_data = (unsigned long) __x64_sys_put_data;       
long sys_get_data = (unsigned long) __x64_sys_get_data;       


int init_system_call(void) {

	int i;
	int ret;


	AUDIT{
		printk("%s: vtpmo received sys_call_table address %px\n",MODNAME,(void*)the_syscall_table);
		printk("%s: initializing - hacked entries %d\n",MODNAME,HACKED_ENTRIES);
	}

	new_sys_call_array[0] = (unsigned long)sys_invalidate_data;
        new_sys_call_array[1] = (unsigned long)sys_put_data;
        new_sys_call_array[2] = (unsigned long)sys_get_data;

        ret = get_entries(restore,HACKED_ENTRIES,(unsigned long*)the_syscall_table,&the_ni_syscall);

        if (ret != HACKED_ENTRIES){
                printk("%s: could not hack %d entries (just %d)\n",MODNAME,HACKED_ENTRIES,ret); 
                return -1;      
        }

	unprotect_memory();

        INVALIDATE = restore[0];
        PUT = restore[1];
        GET = restore[2];
        
        for(i=0;i<HACKED_ENTRIES;i++){
                ((unsigned long *)the_syscall_table)[restore[i]] = (unsigned long)new_sys_call_array[i];
        }

	protect_memory();

        printk("%s: all new system-calls correctly installed on sys-call table\n",MODNAME);

        return 0;

}

void cleanup_system_calls(void) {

        int i;
                
        printk("%s: shutting down\n",MODNAME);

	unprotect_memory();
        for(i=0;i<HACKED_ENTRIES;i++){
                ((unsigned long *)the_syscall_table)[restore[i]] = the_ni_syscall;
        }
	protect_memory();
        printk("%s: sys-call table restored to its original content\n",MODNAME);

        
}


