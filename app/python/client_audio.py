#!/usr/bin/python
import socket
import pyaudio
import wave
import sys

from gmtp import *

#record
CHUNK = 1024
FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 44100
RECORD_SECONDS = 40

default_ip = get_ip_address('eth0')
default_port = 12345
default_address = (default_ip, default_port)

if(len(sys.argv) > 2):
    address = (sys.argv[1], int(sys.argv[2]))
elif(len(sys.argv) > 1):
    address = (sys.argv[1], default_port)
else:
    address = default_address

s = socket.socket(socket.AF_INET, socket.SOCK_GMTP, socket.IPPROTO_GMTP)
print(s)
print(address)

s.connect(address)

p = pyaudio.PyAudio()

stream = p.open(format=FORMAT,
                channels=CHANNELS,
                rate=RATE,
                input=True,
                frames_per_buffer=CHUNK)

print("*recording")

frames = []

for i in range(0, int(RATE/CHUNK*RECORD_SECONDS)):
 data  = stream.read(CHUNK)
 frames.append(data)
 s.sendall(data)

print("*done recording")

stream.stop_stream()
stream.close()
p.terminate()
s.close()

print("*closed")
