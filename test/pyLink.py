#!/usr/bin/python

import sys
import socket
import signal
import time

EXPECTED_ARGS = 2
TIME_TO_SLEEP = 2

def usage():
	print 'Usage:', __file__, '<local-port> <remote-port>'

if len(sys.argv) < EXPECTED_ARGS + 1:
	usage()
	sys.exit(0)

UDP_REMOTE_PORT = 0
UDP_REMOTE_HOST = 'localhost' 

UDP_LOCAL_PORT = 0
UDP_LOCAL_HOST = 'localhost'

UDP_MESSAGE = 'hello world!'

try:
	UDP_LOCAL_PORT = int(sys.argv[1])
	UDP_REMOTE_PORT = int(sys.argv[2])
except:
	print 'Please provide a port number as the first argument'
	sys.exit(0)

# set up the handler for signals
def handler(signum, frame):
	print 'Signal caught. Goodbye!'
	sys.exit(0)

signal.signal(signal.SIGINT, handler)

sfd = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
sfd.bind((UDP_LOCAL_HOST, UDP_LOCAL_PORT))

while True:
	print 'sending...'
	sfd.sendto(UDP_MESSAGE, (UDP_REMOTE_HOST, UDP_REMOTE_PORT))
	time.sleep(TIME_TO_SLEEP)

print 'done!'
