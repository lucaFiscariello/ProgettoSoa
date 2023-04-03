
obj-m += the_system_call.o
the_system_call-objs += system_call.o lib/scth.o lib/block_read_write.o lib/rcu.o 


A = $(shell cat /sys/module/the_usctm/parameters/sys_call_table_address)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

