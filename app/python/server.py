#!/usr/bin/python

import socket
import fcntl
import struct
import time

def get_ip_address(ifname):
	a = 'b'
	s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	return socket.inet_ntoa(fcntl.ioctl(s.fileno(),0x8915,struct.pack('256s', ifname[:15]))[20:24])

SOCK_GMTP = 7
IPPROTO_GMTP = 254

ip = get_ip_address('eth0')

print("Starting server... at ", ip)
address = (ip, 12345)

# Create sockets
server_socket = socket.socket(socket.AF_INET, SOCK_GMTP, IPPROTO_GMTP)
print(server_socket)

# Connect sockets
server_socket.bind(address)

print("Listening...")
time.sleep(1)
server_socket.listen(1)
server_input, address = server_socket.accept()

# Print
while True:
	response = server_input.recv(1024).decode('utf-8')
	if (response != "sair"):
		print ("Mensagem do cliente:", response)
	else:
		server_socket.close()
		break
