PWD := $(shell pwd)
KERNEL:= /lib/modules/`uname -r`/build

KBUILD_EXTRA_SYMBOLS := $(PWD)/Module.symvers

obj-m += gmtp.o gmtp_ipv4.o

gmtp-y += hash.o input.o minisocks.o output.o sockopt.o proto.o timer.o
gmtp-y +=  mcc/mcc_proto.o mcc/mcc_input.o mcc/mcc_equation.o mcc/packet_history.o mcc/loss_interval.o 
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
	/sbin/rmmod -f gmtp_ipv4
	@echo 'Uninstalling gmtp.ko...'
	/sbin/rmmod -f gmtp
	@echo '-------------------------------'
	/sbin/lsmod

clean:
	make -C $(KERNEL) M=$(PWD) clean
	#rm -rf *.o *.ko
