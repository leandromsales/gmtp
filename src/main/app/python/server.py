#!/usr/bin/python

import socket

SOCK_GMTP = 7
IPPROTO_GMTP = 254

address = (socket.gethostname(), 12345)

# Create sockets
#server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.getprotobyname("gmtp"))
server_socket = socket.socket(socket.AF_INET, SOCK_GMTP, IPPROTO_GMTP)
print(server_socket)

# Connect sockets
server_socket.bind(address)
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
