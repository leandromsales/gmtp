PWD := $(shell pwd)
KERNEL:= /lib/modules/`uname -r`/build

KBUILD_EXTRA_SYMBOLS := $(PWD)/Module.symvers

obj-m += gmtp.o gmtp_inter.o

gmtp-y = ../gmtp.o
gmtp_inter-y += ucc.o mcc-inter.o relay.o hash-inter.o build.o output-inter.o input-inter.o gmtp-inter.o

all:
	make -C $(KERNEL) M=$(PWD) modules
	/sbin/modinfo gmtp.ko
	@echo '-------------------------------'
	/sbin/modinfo gmtp_inter.ko

install:
	@echo 'Installing gmtp.ko...'
	/sbin/insmod gmtp.ko
	@echo 'Installing gmtp_ipv4.ko...'
	/sbin/insmod gmtp_inter.ko
	@echo '-------------------------------'
	/sbin/lsmod
	
uninstall:
	@echo 'Uninstalling gmtp_inter.ko...'
	/sbin/rmmod -f gmtp_ipv4
	@echo 'Uninstalling gmtp.ko...'
	/sbin/rmmod -f gmtp
	@echo '-------------------------------'
	/sbin/lsmod

clean:
	make -C $(KERNEL) M=$(PWD) clean
	#rm -rf *.o *.ko
