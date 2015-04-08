#!/usr/bin/python
# Echo server program
import socket
import pyaudio
import wave
import time
import sys

from gmtp import *

CHUNK = 1024
FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 44100
RECORD_SECONDS = 4
WAVE_OUTPUT_FILENAME = "output.wav"
WIDTH = 2
frames = []

p = pyaudio.PyAudio()
stream = p.open(format=p.get_format_from_width(WIDTH),
                channels=CHANNELS,
                rate=RATE,
                output=True,
                frames_per_buffer=CHUNK)


ip = get_ip_address('eth0')
default_port = 12345
default_address = (ip, default_port)

if(len(sys.argv) > 1):
    address = (ip, int(sys.argv[1]))
else:
    address = default_address

print("Starting server... at ", str(address))

# Create sockets
s = socket.socket(socket.AF_INET, socket.SOCK_GMTP, socket.IPPROTO_GMTP)
print(s)

s.bind(address)
s.listen(1)
conn, addr = s.accept()
print 'Connected by', addr
data = conn.recv(1024)

i=1
while data != '':
    stream.write(data)
    data = conn.recv(1024)
    i=i+1
    print i
    frames.append(data)

wf = wave.open(WAVE_OUTPUT_FILENAME, 'wb')
wf.setnchannels(CHANNELS)
wf.setsampwidth(p.get_sample_size(FORMAT))
wf.setframerate(RATE)
wf.writeframes(b''.join(frames))
wf.close()

stream.stop_stream()
stream.close()
p.terminate()
conn.close()
