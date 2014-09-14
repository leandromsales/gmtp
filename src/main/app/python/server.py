#!/usr/bin/python

import socket
import time

SOCK_GMTP = 7
IPPROTO_GMTP = 254

print("Getting address...")
address = (socket.gethostname(), 12345)
print(address)

# Create sockets
#server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.getprotobyname("gmtp"))
print("Vou atribuir agora")
time.sleep(1)
server_socket = socket.socket(socket.AF_INET, SOCK_GMTP, IPPROTO_GMTP)
print(server_socket)

# Connect sockets
print("Pronto. Daqui a 2 segundos, vou fazer o bind")
time.sleep(2)
server_socket.bind(address)

print("Fiz o bind. Agora vou fazer o listen")
time.sleep(1)
server_socket.listen(1)

print("Pronto. Aguardando os clientes")
server_input, address = server_socket.accept()

# Print
while True:
	response = server_input.recv(1024).decode('utf-8')
	if (response != "sair"):
		print ("Mensagem do cliente:", response)
	else:
		server_socket.close()
		break
