# In-tree GMTP. Objects are linked into the kernel image or into liblinux.so.
obj-$(CONFIG_GMTP) += gmtp.o gmtp_ipv4.o

gmtp-y += hash/hash.o hash/hash_client.o hash/hash_server.o hash/client.o
gmtp-y += input.o minisocks.o output.o sockopt.o proto.o timer.o gmtp_hashtables.o
gmtp-y += mcc/mcc_proto.o mcc/mcc_input.o mcc/mcc_equation.o mcc/packet_history.o mcc/loss_interval.o
gmtp_ipv4-y := ipv4.o

obj-$(CONFIG_GMTP_INTER) += gmtp_inter.o
gmtp_inter-y += gmtp-inter/ucc.o gmtp-inter/mcc-inter.o gmtp-inter/relay.o
gmtp_inter-y += gmtp-inter/hash-inter.o gmtp-inter/build.o
gmtp_inter-y += gmtp-inter/output-inter.o gmtp-inter/input-inter.o gmtp-inter/gmtp-inter.o
