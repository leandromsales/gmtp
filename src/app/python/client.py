#!/usr/bin/python

import socket

IPPROTO_GMTP = 254

address = (socket.gethostname(), 12345)

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
