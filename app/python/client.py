#!/usr/bin/python

import sys
import socket
import fcntl
import struct
import time

def get_ip_address(ifname):
    a = 'b'
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    return socket.inet_ntoa(fcntl.ioctl(s.fileno(),0x8915,struct.pack('256s', ifname[:15]))[20:24])

IPPROTO_GMTP = 254

default_ip = get_ip_address('eth0')
default_port = 12345
default_address = (default_ip, default_port)

if(len(sys.argv) > 2):
    address = (sys.argv[1], int(sys.argv[2]))
elif(len(sys.argv) > 1):
    address = (sys.argv[1], default_port)
else:
    address = default_address

# Create sockets
client_socket = socket.socket(socket.AF_INET, 7, IPPROTO_GMTP)
print(client_socket)
print(address)

client_socket.connect(address)

# Echo
while True:
    text = input("Informe texto ou digite 'sair' para desconectar: ")
    client_socket.send(text.encode('utf-8'))
    if (text == "sair"):
        client_socket.close()
        break
