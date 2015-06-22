#!/usr/bin/python

import sys
import socket
import time
import timeit
from datetime import datetime
import threading

from gmtp import *

ip = get_ip_address('eth0')
default_port = 12345
default_address = (ip, default_port)

msg = "Welcome to the jungle!"
out = "sair"

if(len(sys.argv) > 1):
    address = (ip, int(sys.argv[1]))
else:
    address = default_address

print "Starting server... at ", address

class ClientThread(threading.Thread):

    def __init__(self, address, socket, text):
        threading.Thread.__init__(self)
        self.i = 0
        self.ip = address[0]
        self.port = address[1]
        self.socket = socket
        self.text = text
        
        self.total_size = 0
        self.last_size = 0;
        
        self.total_time = 0
        self.start_time = 0
        self.last_time = 0
        
        print "[+] New thread started for", self.ip, ":", str(self.port)

    def run(self):
        print "Sending text: '" + self.text + "' at " +  str(tx_rate) + " bytes/s\n"
        print "Sending... "
        
        while True: 
            
            time.sleep(0.00175); # App controls Tx    0,001 ~ 100.000 bytes/s
                  
            if(self.text != out):
                self.i = self.i + 1
                if(self.i == 1):
                    self.start_time = timeit.default_timer()
                    self.last_time = timeit.default_timer()
                
                text = self.text + " (" + str(self.i) + ")"
                size = getPacketSize(text)
                self.total_size = self.total_size + size
                
                self.socket.send(text.encode('utf-8'))
                
                if(self.i%25 == 0):
                    sys.stdout.write("=>")
                    sys.stdout.flush()
                
                if(self.i%1000 == 0):
                    nowstr = str(datetime.now().strftime('%H:%M:%S:%f'))
                    
                    self.total_time = timeit.default_timer() - self.start_time
                    elapsed = timeit.default_timer() - self.last_time
                    size1000 = self.total_size - self.last_size
                    
                    rate = "%.2f" % (size1000/elapsed)
                    total_rate = "%.2f" % (self.total_size/self.total_time)
                    
                    self.last_time = timeit.default_timer()
                    self.last_size = self.total_size
                    
                    print "\nMessage", self.i, "sent to client at", nowstr +":\n", text
                    print "\tPacket Size: ", size, "bytes"
                    print "\tSize of last 1000:", size1000, "bytes / Time elapsed: ", elapsed, "s"
                    print "\tTotal sent:", self.total_size, "bytes / Total time:", self.total_time, "s"
                    print "\tSend rate (last 1000):", rate, "bytes/s"
                    print "\tSend rate (total): ", total_rate, "bytes/s\n\n"
                    print "Sending... "
        
            else:
                self.socket.send(self.text.encode('utf-8'))
                self.socket.close()
                break

        print "Client disconnected..."
        
# Create sockets
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)


tx_rate = 50000 #bytes/s.  250000 bytes/s == 2 Mbps
#server_socket.setsockopt(socket.SOL_GMTP, socket.GMTP_SOCKOPT_MAX_TX_RATE, tx_rate)

# Connect sockets
server_socket.bind(address)
threads = []
print "Listening for incoming connections...\n"
time.sleep(0.5)

def join_threads(list):
    for t in list:
        t.join()

try:
    while True:
        server_socket.listen(20)
        client_input, client_addr = server_socket.accept()
        newthread = ClientThread(client_addr, client_input, msg)
        newthread.start()
        threads.append(newthread)
except (KeyboardInterrupt, SystemExit):
    print '\nReceived keyboard interrupt, quitting threads...\n'
    for t in threads:
        t.text = out
    join_threads(threads)
    server_socket.close()
    sys.exit()
    

