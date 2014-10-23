from random import randrange
from uuid import uuid4
def genIpUnicast():
	not_valid = [10,127,169,172,192]

	first = randrange(1,256)
	while first in not_valid:
		first = randrange(1,256)
 
	ip = ".".join([str(first),str(randrange(1,256)),str(randrange(1,256)),str(randrange(1,256))])
	return ip

def genIpMulticast():
 
	ip = ".".join(['239','192',str(randrange(0,256)),str(randrange(0,256))])
	return ip

def genPort():
	return randrange(1024,65536)

print("")
print(uuid4())
print("")
print(genIpUnicast())
print("")
print(genIpUnicast())
print("")
print(genPort())
print("")
print(genIpMulticast())
print("")
print(genPort())
print("")
