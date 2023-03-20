
all:
	gcc singlefile-FS/singlefilemakefs.c -o singlefile-FS/singlefilemakefs
	gcc system-call/user/user.c -o system-call/user/user
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/singlefile-FS modules
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/Linux-sys_call_table-discoverer-master modules 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/major-minor-management modules 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/system-call modules 

create-fs:
	dd bs=4096 count=100 if=/dev/zero of=singlefile-FS/image
	./singlefile-FS/singlefilemakefs singlefile-FS/image
	sudo mknod /dev/myDev c 237 0

mount-mod:
	sudo insmod singlefile-FS/singlefilefs.ko
	sudo insmod Linux-sys_call_table-discoverer-master/the_usctm.ko
	sudo insmod major-minor-management/baseline-char-dev.ko

mount-sc:
	sudo insmod system-call/the_virtual-to-physical-memory-mapper.ko the_syscall_table=$(shell sudo cat /sys/module/the_usctm/parameters/sys_call_table_address)

mount-fs:
	mkdir mount
	sudo mount -o loop -t singlefilefs singlefile-FS/image ./mount/


remove-fs:
	sudo rm /dev/myDev

umount-fs:
	sudo umount ./mount/
	rm -r ./mount/

umount-mod:
	sudo rmmod singlefilefs
	sudo rmmod the_usctm
	sudo rmmod baseline-char-dev
	sudo rmmod the_virtual-to-physical-memory-mapper

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/singlefile-FS clean
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/Linux-sys_call_table-discoverer-master clean 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/major-minor-management clean 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/system-call clean 

	rm singlefile-FS/singlefilemakefs




