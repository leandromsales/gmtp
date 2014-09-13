PWD := $(shell pwd)
#KERNEL := /usr/src/linux
KERNEL:= /lib/modules/`uname -r`/build

KBUILD_EXTRA_SYMBOLS := $(PWD)/Module.symvers

MOD := gmtp
obj-m += $(MOD).o  
$(MOD)-objs += main.o proto.o output.o options.o

all:
	make -C $(KERNEL) M=$(PWD) modules
	modinfo $(MOD).ko

clean:
	make -C $(KERNEL) M=$(PWD) clean
	#rm -rf *.o *.ko
