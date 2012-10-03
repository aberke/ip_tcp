#!/usr/bin/python

import sys
import socket
import signal
import time
import utils

EXPECTED_ARGS = 1
TIME_TO_SLEEP = 2

def usage():
	print 'Usage:', __file__, '<local-port> <remote-port>'

if len(sys.argv) < EXPECTED_ARGS + 1:
	usage()
	sys.exit(0)

interface = ''
try:
	f = open(sys.argv[1])
	line = f.readline()
	interface = utils.Interface(line)
except IOError:
	utils.error("Unable to open file: " + sys.argv[1])

interface.interface_print()

# set up the handler for signals
def handler(signum, frame):
	print 'Signal caught. Goodbye!'
	sys.exit(0)

signal.signal(signal.SIGINT, handler)

while True:
	print 'sending...'
	interface.send('Hello world!')
	time.sleep(TIME_TO_SLEEP)

print 'done!'
