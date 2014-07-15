PWD := $(shell pwd)
#KERNEL:= $(shell uname -r)
KERNEL := /usr/src/linux

#INC = -I"include"

MOD := gmtp

obj-m += $(MOD).o

all:
	make -C $(KERNEL) M=$(PWD) modules
	modinfo $(MOD).ko

clean:
	make -C $(KERNEL) M=$(PWD) clean
