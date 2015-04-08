#!/usr/bin/python

import sys
import socket
import time
import timeit
from datetime import datetime

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
        
# Create sockets
server_socket = socket.socket(socket.AF_INET, socket.SOCK_GMTP, socket.IPPROTO_GMTP)
server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

flowname = getHash(address)
server_socket.setsockopt(socket.SOL_GMTP, socket.GMTP_SOCKOPT_FLOWNAME, flowname)

#tx_rate = "max_gmtp_rate"
tx_rate = 180000 #bytes/s.  250000 bytes/s == 2 Mbps
server_socket.setsockopt(socket.SOL_GMTP, socket.GMTP_SOCKOPT_MAX_TX_RATE, tx_rate)

# Connect sockets
server_socket.bind(address)
threads = []
print "Listening for incoming connections...\n"
time.sleep(0.5)

server_socket.listen(4)
server_output, client_addr = server_socket.accept()
print "[+] Client connected:", client_addr[0], ":", str(client_addr[1])

i = 0
total_size = 0
last_size = 0;
        
total_time = 0
start_time = 0
last_time = 0
 
print "Sending text: '" + msg + "' at " +  str(tx_rate) + " bytes/s (max)\n"
print "Sending... "

try:
    while True:
        
        #time.sleep(0.001); # App controls Tx    0,001 ~ 100.000 bytes/s
                  
        i = i + 1
        if(i == 1):
            start_time = timeit.default_timer()
            last_time = timeit.default_timer()
        
        text = msg + " (" + str(i) + ")"
        size = getPacketSize(text)
        total_size = total_size + size
        
        server_output.send(text.encode('utf-8'))
        
        if(i%25 == 0):
            sys.stdout.write("=>")
            sys.stdout.flush()
        
        if(i%1000 == 0):
            nowstr = str(datetime.now().strftime('%H:%M:%S:%f'))
            
            total_time = timeit.default_timer() - start_time
            elapsed = timeit.default_timer() - last_time
            size1000 = total_size - last_size
            
            rate = "%.2f" % (size1000/elapsed)
            total_rate = "%.2f" % (total_size/total_time)
            
            last_time = timeit.default_timer()
            last_size = total_size
            
            print "\nMessage", i, "sent to client at", nowstr +":\n", text
            print "\tPacket Size: ", size, "bytes"
            print "\tSize of last 1000:", size1000, "bytes / Time elapsed: ", elapsed, "s"
            print "\tTotal sent:", total_size, "bytes / Total time:", total_time, "s"
            print "\tSend rate (last 1000):", rate, "bytes/s"
            print "\tSend rate (total): ", total_rate, "bytes/s\n\n"
            print "Sending... "
        
except (KeyboardInterrupt, SystemExit):
    print '\nReceived keyboard interrupt, quitting...\n'
    server_output.send(out.encode('utf-8'))
    server_output.close()
    server_socket.close()
    

