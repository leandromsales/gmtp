#!/usr/bin/python
# coding: utf-8
# GMTP Module

import socket
import fcntl
import struct
import md5

GMTP_HDR_LEN = 36
IP_HDR_LEN = 20
ETH_HDR_LEN = 14

socket.SOCK_GMTP = 7
socket.IPPROTO_GMTP = 254
socket.SOL_GMTP = 300

socket.GMTP_SOCKOPT_FLOWNAME = 1
socket.GMTP_SOCKOPT_MAX_TX_RATE = 2
socket.GMTP_SOCKOPT_UCC_TX_RATE = 3
socket.GMTP_SOCKOPT_GET_CUR_MSS = 4
socket.GMTP_SOCKOPT_SERVER_RTT = 5
socket.GMTP_SOCKOPT_SERVER_TIMEWAIT = 6
socket.GMTP_SOCKOPT_PULL = 7
socket.GMTP_SOCKOPT_ROLE_RELAY = 8
socket.GMTP_SOCKOPT_RELAY_ENABLED = 9

def get_ip_address(ifname):
	a = 'b'
	s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	return socket.inet_ntoa(fcntl.ioctl(s.fileno(),0x8915,struct.pack('256s', ifname[:15]))[20:24])

def createHash(host, port):
	m = md5.new()
	m.update(str(host) + ":" + str(port))
	print "Hash: ", m.hexdigest()
	return m.digest()

def getHash(address):
	host, port = address
	return createHash(host, port)

def getPacketSize(data):
	return len(str(data)) + GMTP_HDR_LEN + IP_HDR_LEN + ETH_HDR_LEN
