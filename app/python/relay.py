#!/usr/bin/python
import socket
import sys
from gmtp import *

relay_enabled = 1

if(len(sys.argv) > 1):
    relay_enabled = int(sys.argv[1])

print "Creating GMTP socket" 
relay_socket = socket.socket(socket.AF_INET, socket.SOCK_GMTP, socket.IPPROTO_GMTP)

print "Setting GMTP socket to 'RELAY'" 
relay_socket.setsockopt(socket.SOL_GMTP, socket.GMTP_SOCKOPT_ROLE_RELAY, 1)

print "Setting GMTP Relay to", relay_enabled 
relay_socket.setsockopt(socket.SOL_GMTP, socket.GMTP_SOCKOPT_RELAY_ENABLED, relay_enabled)

print "Exiting...\n"