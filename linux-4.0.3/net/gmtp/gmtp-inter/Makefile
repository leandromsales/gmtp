PWD := $(shell pwd)
KERNEL:= /lib/modules/`uname -r`/build
KBUILD_EXTRA_SYMBOLS := $(PWD)/Module.symvers

obj-m += gmtp.o gmtp_inter.o

gmtp-y += ../gmtp.o 
gmtp_inter-y +=  ucc.o mcc-inter.o hash-inter.o output-inter.o input-inter.o gmtp-inter.o

all:
	$(MAKE) -C $(KERNEL) M=$(PWD) modules
	/sbin/modinfo gmtp_inter.ko
	
install:
	@echo 'Installing gmtp_inter.ko...'
	/sbin/insmod gmtp_inter.ko
	@echo '-------------------------------'
	/sbin/lsmod
	
uninstall:
	@echo 'Uninstalling gmtp_inter...'
	/sbin/rmmod -f gmtp_inter
	@echo '-------------------------------'
	/sbin/lsmod

clean:
	make -C $(KERNEL) M=$(PWD) clean
