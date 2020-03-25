#!/usr/bin/python

import sys
import socket
import time
import timeit
from datetime import datetime
from sys import stdout
from optparse import Option, OptionParser
from gmtp import *

default_port = 12345

usage = "usage: %prog [options]. Note: -a takes precedence of -i, specify one or other."
parser = OptionParser(usage=usage, version="%prog 1.0")

parser.add_option("-i", "--iface", dest="iface",
                  help="The network interface to bind.", metavar="IFACE")
parser.add_option("-a", "--address", dest="address",
                  help="The network address", metavar="ADDRESS")
parser.add_option("-p", "--port", dest="port", type="int",
                  help="The network port [default: %default]", default=default_port, metavar="PORT")

(options, args) = parser.parse_args()

if (options.address):
    address = (options.address, options.port)
elif (options.iface):
    ip = get_ip_address(options.iface)
    address = (ip, options.port)
else:
    parser.print_help()
    sys.exit(1)

# Create sockets
client_socket = socket.socket(socket.AF_INET, socket.SOCK_GMTP, socket.IPPROTO_GMTP)

# flowname = "1234567812345678"
# client_socket.setsockopt(socket.SOL_GMTP, socket.GMTP_SOCKOPT_FLOWNAME, flowname)

print 'Connecting to ', address
client_socket.connect(address)
print 'Connected!'

print '\nCaution! Client is printing only each 100 messages!\n'

i = 0
total_size = 0
lastsize100 = 0

logfilename = "logs/logclient_" +  str(timeit.default_timer())[4:] + ".log" 
logfile = open(logfilename, 'w+')


logtable = "seq\ttime\tsize\telapsed\tinst_rate" + \
"\tsize100\telapsed100\trate100" + \
"\ttotal_size\ttotal_time\ttotal_rate\r\n\r\n";
logfile.write(logtable)

start_time = 0
last_time = 0
last_time100 = 0

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
            last_time100 = timeit.default_timer()
     
        total_time = timeit.default_timer() - start_time
        elapsed = timeit.default_timer() - last_time
        last_time = timeit.default_timer()
    
        if(elapsed == 0 or total_time == 0): #avoid division by zero...
            continue
    
        size = getPacketSize(message)
        total_size = total_size + size
    
        instant_rate = "%.2f" % (size/elapsed)
        total_rate = "%.2f" % (total_size/total_time)
        
        strsize100 = " "
        strtime100 = " "
        strrate100 = " "
    
        nowstr = str(datetime.now().strftime('%H:%M:%S:%f'))
    
        if(i%50 == 0):
            stdout.write("=>")
            stdout.flush()
    
        #if(i%1000 == 0): #print less msgs...              
        if(i <= 10 or i%100 == 0):
            size100 = total_size - lastsize100
            lastsize100 = total_size
            time100 = timeit.default_timer() - last_time100
            last_time100 = timeit.default_timer()

            rate100 = 0
            if time100 != 0:
                rate100 = size100/time100
        
            strsize100 = str(size100)
            strtime100 = str(time100)
            strrate100 = "%.2f" % (rate100)
    
            print "\nMessage", i, "received from server at", nowstr +":\n", message
            print "\tPacket Size: ", size, "bytes / Time elapsed:", elapsed, "s"
            print "\tSize (last 100):", strsize100, "bytes / Time elapsed (last 100):", strtime100, "s"
            print "\tTotal received:", total_size, "bytes / Total time:", total_time, "s"
            print "\tRate (instant):", instant_rate, "bytes/s"
            print "\tRate (last 100):", strrate100, "bytes/s"
            print "\tRate (total):  ", total_rate, "bytes/s\n\n"
            print "Receiving... "
    
        logtext = str(i) + "\t" + nowstr + "\t" + \
                str(size) + "\t" + str(elapsed) + "\t" + instant_rate + \
                "\t" + strsize100 + "\t" + strtime100 + "\t" + strrate100 + \
                "\t" + str(total_size) + "\t" + str(total_time) + \
                "\t" + total_rate + "\r\n"
    
        if(i>100):
            logfile.write(logtext)
        
except (KeyboardInterrupt):
    print '\nReceived keyboard interrupt, quitting...\n'
finally:
    logfile.close()
    client_socket.close()

    
