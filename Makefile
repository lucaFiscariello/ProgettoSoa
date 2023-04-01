
all:
	gcc singlefile-FS/singlefilemakefs.c -o singlefile-FS/singlefilemakefs
	gcc system-call/user/user.c -o system-call/user/user
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/singlefile-FS modules
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/Linux-sys_call_table-discoverer-master modules 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/device-driver modules 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/system-call modules 

create-fs:
	dd bs=4096 count=100 if=/dev/zero of=singlefile-FS/image
	./singlefile-FS/singlefilemakefs singlefile-FS/image
	sudo mknod /dev/myDev c 237 0

mount-mod:
	sudo insmod singlefile-FS/singlefilefs.ko
	sudo insmod Linux-sys_call_table-discoverer-master/the_usctm.ko

mount-sys:
	sudo insmod system-call/the_system_call.ko the_syscall_table=$(shell sudo cat /sys/module/the_usctm/parameters/sys_call_table_address)
	sudo insmod device-driver/char-dev.ko adress_dev_mount=$(shell sudo cat /sys/module/singlefilefs/parameters/adress_dev_mount)

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
	sudo rmmod char-dev
	sudo rmmod the_system_call

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/singlefile-FS clean
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/Linux-sys_call_table-discoverer-master clean 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/device-driver clean 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)/system-call clean 

	rm singlefile-FS/singlefilemakefs




