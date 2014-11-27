PWD := $(shell pwd)
#KERNEL := /usr/src/linux
KERNEL:= /lib/modules/`uname -r`/build

KBUILD_EXTRA_SYMBOLS := $(PWD)/Module.symvers

obj-m += gmtp.o gmtp_ipv4.o

gmtp-y = input.o output.o proto.o
gmtp_ipv4-y := ipv4.o

all:
	make -C $(KERNEL) M=$(PWD) modules
	/sbin/modinfo gmtp.ko
	@echo '-------------------------------'
	/sbin/modinfo gmtp_ipv4.ko

install:
	@echo 'Installing gmtp.ko...'
	/sbin/insmod gmtp.ko
	@echo 'Installing gmtp_ipv4.ko...'
	/sbin/insmod gmtp_ipv4.ko
	@echo '-------------------------------'
	/sbin/lsmod
	
uninstall:
	@echo 'Uninstalling gmtp_ipv4.ko...'
	/sbin/rmmod gmtp_ipv4.ko
	@echo 'Uninstalling gmtp.ko...'
	/sbin/rmmod gmtp.ko
	@echo '-------------------------------'
	/sbin/lsmod

clean:
	make -C $(KERNEL) M=$(PWD) clean
	#rm -rf *.o *.ko
