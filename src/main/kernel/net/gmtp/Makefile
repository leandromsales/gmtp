PWD := $(shell pwd)
#KERNEL := /usr/src/linux
KERNEL:= /lib/modules/`uname -r`/build

KBUILD_EXTRA_SYMBOLS := $(PWD)/Module.symvers

obj-m += gmtp.o gmtp_ipv4.o

gmtp-y = output.o proto.o
gmtp_ipv4-y := ipv4.o

all:
	make -C $(KERNEL) M=$(PWD) modules
	/sbin/modinfo gmtp.ko
	@echo '-------------------------------'
	/sbin/modinfo gmtp_ipv4.ko

clean:
	make -C $(KERNEL) M=$(PWD) clean
	#rm -rf *.o *.ko
