#!/usr/bin/python

import socket
import fcntl
import struct
import time

def get_ip_address(ifname):
	a = 'b'
	s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	return socket.inet_ntoa(fcntl.ioctl(s.fileno(),0x8915,struct.pack('256s', ifname[:15]))[20:24])

IPPROTO_GMTP = 254

ip_address = get_ip_address('wlan0')
#ip_address = "172.20.9.89"

address = (ip_address, 12345)

# Create sockets
client_socket = socket.socket(socket.AF_INET, 7, IPPROTO_GMTP)
print(client_socket)

client_socket.connect(address)

# Echo
while True:
	text = input("Informe texto ou digite 'sair' para desconectar: ")
	client_socket.send(text.encode('utf-8'))
	if (text == "sair"):
		client_socket.close()
		break
