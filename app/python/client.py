#!/usr/bin/python

import sys
import socket
import time
import timeit
from datetime import datetime

from gmtp import *
from __builtin__ import str
from twisted.python.dist import getPackages
from sys import stdout
from ubuntu_sso.utils.ui import TRY_AGAIN_BUTTON
from _dbus_bindings import Message

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
client_socket = socket.socket(socket.AF_INET, socket.SOCK_GMTP, socket.IPPROTO_GMTP)

flowname = getHash(address)
client_socket.setsockopt(socket.SOL_GMTP, socket.GMTP_SOCKOPT_FLOWNAME, flowname)

print 'Connecting to ', address
client_socket.connect(address)
print 'Connected!'

print '\nCaution! Client is printing only each 1.000 messages!\n'

i = 0
total_size = 0
lastsize1000 = 0

logfilename = "logs/logclient_" +  str(timeit.default_timer())[4:] + ".log" 
logfile = open(logfilename, 'w')

logtable = "seq\ttime\tsize\telapsed\tinst_rate" + \
"\tsize1000\telapsed1000\trate1000" + \
"\ttotal_size\ttotal_time\ttotal_rate\r\n\r\n";
logfile.write(logtable)

start_time = 0
last_time = 0
last_time1000 = 0

print "Receiving... "
message = ' '
out = "sair"
 
try:
    while message != out and len(message) > 0:
    
        message = client_socket.recv(1024).decode('utf-8')
        message = message.encode("ascii", "ignore")
    
        if(message == out):
            print "\nEnd:", "'" + out + "'", "message received!\n"
        elif(len(message) <= 0):
            print "\nEnd: len(message) <= 0\n"
    
        i = i + 1
    
        if(i == 1):
            start_time = timeit.default_timer()
            last_time = timeit.default_timer()
            last_time1000 = timeit.default_timer()
     
        total_time = timeit.default_timer() - start_time
        elapsed = timeit.default_timer() - last_time
        last_time = timeit.default_timer()
    
        if(elapsed == 0): #avoid division by zero...
            continue
    
        size = getPacketSize(message)
        total_size = total_size + size
    
        instant_rate = "%.2f" % (size/elapsed)
        total_rate = "%.2f" % (total_size/total_time)
        
        strsize1000 = " "
        strtime1000 = " "
        strrate1000 = " "
    
        nowstr = str(datetime.now().strftime('%H:%M:%S:%f'))
    
        if(i%25 == 0):
            stdout.write("=>")
            stdout.flush()
    
        if(i%1000 == 0): #print less msgs...              
        
            size1000 = total_size - lastsize1000
            lastsize1000 = total_size
            time1000 = timeit.default_timer() - last_time1000
            last_time1000 = timeit.default_timer()
            rate1000 = size1000/time1000
        
            strsize1000 = str(size1000)
            strtime1000 = str(time1000)
            strrate1000 = "%.2f" % (rate1000)
    
            print "\nMessage", i, "received from server at", nowstr +":\n", message
            print "\tPacket Size: ", size, "bytes / Time elapsed:", elapsed, "s"
            print "\tSize (last 1000):", strsize1000, "bytes / Time elapsed (last 1000):", strtime1000, "s"
            print "\tTotal received:", total_size, "bytes / Total time:", total_time, "s"
            print "\tRate (instant):", instant_rate, "bytes/s"
            print "\tRate (last 1000):", strrate1000, "bytes/s"
            print "\tRate (total):  ", total_rate, "bytes/s\n\n"
            print "Receiving... "
    
        logtext = str(i) + "\t" + nowstr + "\t" + \
                str(size) + "\t" + str(elapsed) + "\t" + instant_rate + \
                "\t" + strsize1000 + "\t" + strtime1000 + "\t" + strrate1000 + \
                "\t" + str(total_size) + "\t" + str(total_time) + \
                "\t" + total_rate + "\r\n"
    
        if(i>1000):
            logfile.write(logtext)
        
except (KeyboardInterrupt):
    print '\nReceived keyboard interrupt, quitting...\n'
finally:
    logfile.close()
    client_socket.close()

    
