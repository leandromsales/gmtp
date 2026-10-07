obj-$(CONFIG_GMTP) += gmtp.o gmtp_ipv4.o

gmtp-y += hash/hash.o hash/client.o hash/hash_client.o hash/hash_server.o
gmtp-y += input.o minisocks.o output.o sockopt.o proto.o timer.o
gmtp-y +=  mcc/mcc_proto.o mcc/mcc_input.o mcc/mcc_equation.o mcc/packet_history.o mcc/loss_interval.o 

gmtp_ipv4-y := ipv4.o

obj-$(CONFIG_GMTP_INTER) += gmtp-inter/