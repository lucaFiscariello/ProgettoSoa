
num_block= 100

all:
	gcc message-service/singlefile-FS/singlefilemakefs.c -o message-service/singlefile-FS/singlefilemakefs
	gcc message-service/system-call/user/user.c -o message-service/system-call/user/user
	gcc message-service/system-call/user/test.c -lpthread -o message-service/system-call/user/test
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/Linux-sys_call_table-discoverer-master modules 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/message-service modules 

create-fs:
	dd bs=4096 count=$(num_block) if=/dev/zero of=message-service/singlefile-FS/image
	./message-service/singlefile-FS/singlefilemakefs message-service/singlefile-FS/image

mount-mod:
	sudo insmod Linux-sys_call_table-discoverer-master/the_usctm.ko

mount-sys:
	sudo insmod message-service/the-message-service.ko the_syscall_table=$(shell sudo cat /sys/module/the_usctm/parameters/sys_call_table_address) blocks_number=$(num_block)

mount-fs:
	mkdir mount
	sudo mount -o loop -t singlefilefs message-service/singlefile-FS/image ./mount/



umount-fs:
	sudo umount ./mount/
	rm -r ./mount/

umount-mod:
	sudo rmmod the_usctm
	sudo rmmod the-message-service

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/Linux-sys_call_table-discoverer-master clean 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/message-service clean 

	rm message-service/singlefile-FS/singlefilemakefs

user:
	./message-service/system-call/user/user $(shell sudo cat /sys/module/the_message_service/parameters/PUT) $(shell sudo cat /sys/module/the_message_service/parameters/GET) $(shell sudo cat /sys/module/the_message_service/parameters/INVALIDATE)

test:
	./message-service/system-call/user/test $(shell sudo cat /sys/module/the_message_service/parameters/PUT) $(shell sudo cat /sys/module/the_message_service/parameters/GET) $(shell sudo cat /sys/module/the_message_service/parameters/INVALIDATE)

mount-all:
	make create-fs
	make mount-mod
	make mount-sys
	make mount-fs


umount-all:
	make umount-fs
	make umount-mod
	
	



