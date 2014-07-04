PWD := $(shell pwd)
#KERNEL:= $(shell uname -r)
KERNEL := /usr/src/linux

MOD := gmtp

obj-m += $(MOD).o

all:
	make -C $(KERNEL) M=$(PWD) modules
	modinfo $(MOD).ko

clean:
	make -C $(KERNEL) M=$(PWD) clean
