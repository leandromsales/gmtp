obj-$(CONFIG_IP_GMTP) += gmtp.o gmtp_ipv4.o

#gmtp.o 

gmtp-y := ccid.o feat.o input.o minisocks.o options.o output.o proto.o timer.o \
	  qpolicy.o sysctl.o
#
# CCID algorithms to be used by gmtp.ko
#
# CCID-2 is default and has Ack Vectors as dependency
gmtp-y += ccids/ccid2.o ackvec.o	

gmtp_ipv4-y := ipv4.o

#gmtp-$(CONFIG_SYSCTL) += sysctl.o

obj-m += gmtp_ipv4.o

#gmtp.o
