/*
* 
* This is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* This module is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* @file virtual-to-physical-memory-mapper.c 
* @brief This is the main source for the Linux Kernel Module which implements
*       a system call that can be used to query the kernel for current mappings of virtual pages to 
*	physical frames - this service is x86_64 specific in the curent implementation
*
* @author Francesco Quaglia
*
* @date March 25, 2021
*/

#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/apic.h>
#include <asm/io.h>
#include <linux/syscalls.h>
#include "lib/include/scth.h"

#include "lib/include/rcu.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Francesco Quaglia <francesco.quaglia@uniroma2.it>");
MODULE_DESCRIPTION("virtual to physical page mapping oracle");

#define MODNAME "VTPMO"

unsigned long adress_dev_mount = 0x0;
module_param(adress_dev_mount,ulong,0660);

unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);


unsigned long the_ni_syscall;

unsigned long new_sys_call_array[] = {0x0};//please set to sys_vtpmo at startup
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};


#define NO_MAP (-EFAULT)
#define AUDIT if(1)



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _vtpmo, unsigned long, vaddr){
#else
asmlinkage long sys_vtpmo(unsigned long vaddr){
#endif

	printk("%s: Sono stato invocato!",MODNAME);

	return 0;
	
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_vtpmo = (unsigned long) __x64_sys_vtpmo;       
#else
#endif


int init_module(void) {

        void **p = (void*) adress_dev_mount;
        char* devName = *p;
        
        printk("%s: device name %s\n",MODNAME, devName);

        /*
        write_meta_block(devName);
        read_meta_block(devName);
        
        struct block *blockwrite = kmalloc(DIM_BLOCK,GFP_KERNEL);
        struct block *blockRead = kmalloc(DIM_BLOCK,GFP_KERNEL);

        char* testo = "prova testo";
        strncpy(blockwrite->data, testo,12);
        write(devName,3,blockwrite);
        read(devName,3,blockRead);

        printk("Testo letto: %s",blockRead->data);
        */


        set_block_device_onMount(devName);
        inizialize_meta_block();       

        struct block *blockwrite = kmalloc(DIM_BLOCK,GFP_KERNEL);
        struct block *blockRead = kmalloc(DIM_BLOCK,GFP_KERNEL);
        struct block *blockRead2 = kmalloc(DIM_BLOCK,GFP_KERNEL);

        int num_block_read;

        char* testo = "prova testo1";
        char* testo2 = "prova testo2";
        char* testo3 = "prova testo3";
        char* testo4 = "prova testo4";
        char* testo5 = "prova testo5";

        num_block_read= write_rcu(testo);
        read_block_rcu(num_block_read,blockRead);

        num_block_read= write_rcu(testo2);
        read_block_rcu(num_block_read,blockRead2);

        write_rcu(testo3);
        write_rcu(testo4);
        write_rcu(testo5);
        
        struct meta_block_rcu* meta_block_rcu;
        meta_block_rcu = read_ram_metablk();

        printk("Testo letto1: %s",blockRead->data);
        printk("Testo letto2: %s",blockRead2->data);

        read_all_block_rcu(NULL);


	int i;
	int ret;

	AUDIT{
		printk("%s: vtpmo received sys_call_table address %px\n",MODNAME,(void*)the_syscall_table);
		printk("%s: initializing - hacked entries %d\n",MODNAME,HACKED_ENTRIES);
	}

	new_sys_call_array[0] = (unsigned long)sys_vtpmo;

        ret = get_entries(restore,HACKED_ENTRIES,(unsigned long*)the_syscall_table,&the_ni_syscall);

        if (ret != HACKED_ENTRIES){
                printk("%s: could not hack %d entries (just %d)\n",MODNAME,HACKED_ENTRIES,ret); 
                return -1;      
        }

	unprotect_memory();

        for(i=0;i<HACKED_ENTRIES;i++){
                ((unsigned long *)the_syscall_table)[restore[i]] = (unsigned long)new_sys_call_array[i];
        }

	protect_memory();

        printk("%s: all new system-calls correctly installed on sys-call table\n",MODNAME);

        return 0;

}

void cleanup_module(void) {

        int i;
                
        printk("%s: shutting down\n",MODNAME);

	unprotect_memory();
        for(i=0;i<HACKED_ENTRIES;i++){
                ((unsigned long *)the_syscall_table)[restore[i]] = the_ni_syscall;
        }
	protect_memory();
        printk("%s: sys-call table restored to its original content\n",MODNAME);
        
}
