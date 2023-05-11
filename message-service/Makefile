
obj-m += the-message-service.o
the-message-service-objs += message-service.o system-call/system-call.o system-call/lib/scth.o core-RCU/block_read_write.o core-RCU/rcu.o core-RCU/meta_block.o  singlefile-FS/singlefilefs_src.o singlefile-FS/file.o singlefile-FS/dir.o 


all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

