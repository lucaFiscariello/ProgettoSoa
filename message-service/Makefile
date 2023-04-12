
obj-m += the-message-service.o
the-message-service-objs += message-service.o system-call/system-call.o system-call/lib/scth.o core-RCU/lib/block_read_write.o core-RCU/lib/rcu.o core-RCU/lib/meta_block.o  singlefile-FS/singlefilefs_src.o singlefile-FS/file.o singlefile-FS/dir.o device-driver/char-dev.o


all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

