PWD := $(shell pwd)
KERNEL:= /lib/modules/`uname -r`/build
KBUILD_EXTRA_SYMBOLS := $(PWD)/Module.symvers

obj-m += gmtp.o gmtp_intra.o

gmtp-y = ../gmtp/gmtp.o
gmtp_intra-y := gmtp-ucc.o hash.o output.o input.o gmtp-intra.o

all:
	$(MAKE) -C $(KERNEL) M=$(PWD) modules
	/sbin/modinfo gmtp_intra.ko
	
install:
	@echo 'Installing gmtp_intra.ko...'
	/sbin/insmod gmtp_intra.ko
	@echo '-------------------------------'
	/sbin/lsmod
	
uninstall:
	@echo 'Uninstalling gmtp_intra...'
	/sbin/rmmod -f gmtp_intra
	@echo '-------------------------------'
	/sbin/lsmod

clean:
	make -C $(KERNEL) M=$(PWD) clean
