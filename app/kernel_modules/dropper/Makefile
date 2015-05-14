PWD := $(shell pwd)
KERNEL:= /lib/modules/`uname -r`/build
KBUILD_EXTRA_SYMBOLS := $(PWD)/Module.symvers

obj-m += dropper.o


all:
	$(MAKE) -C $(KERNEL) M=$(PWD) modules
	/sbin/modinfo dropper.ko
