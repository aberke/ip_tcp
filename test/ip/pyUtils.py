#!/usr/bin/python

import re
import sys
import socket
import struct

def quit():
	sys.exit(0)

def error(msg):
	print "Error: %s\n" % msg
	quit()

def convert_to_ip(string):
	nums = string.split('.')
	if len(nums) != 4:
		error('Expected ip address string as num.num.num.num, got: ' + string)	

	result = 0
	try:
		for i in range(len(nums)):
			result += int(nums[i])*16**(3-i)
	except ValueError:
		error('Got non-integer in convert_to_ip():' + string)
		
	return result

def pack_char(a, b):
	if a > 2**4-1 or b > 2**4-1 or a < 0 or b < 0:
		error('Invalid arguments to pack_char, expected two integers between 0 and 15, inclusive')
	
	result = chr(a*2**4 + b)
	print 'pack_char returning from (' + str(a) + ',' + str(b) + '):', result
	return result

def ip_create(header_length, version, tos, length, ID, off, ttl, p, ip_sum, ip_src, ip_dst):
	packet = struct.pack('cchhhcchLL', pack_char(header_length, version), chr(tos), length, ID, off, chr(ttl), chr(p), ip_sum, ip_src, ip_dst)
	return packet

def default_ip_packet(msg):
	header_length = 5
	version = 4
	tos = 0
	length = 20 + len(msg)
	ID = -1 # for fragmentation, whatever
	off = -1 # ditto
	ttl = 64
	p   = 6 # tcp
	ip_sum = 10
	ip_src = convert_to_ip('127.18.0.20')
	ip_dst = convert_to_ip('127.18.1.21')

	return ip_create(header_length, version, tos, length, ID, off, ttl, p, ip_sum, ip_src, ip_dst)

lnx_pattern = re.compile(r'(?P<hostlocal>\w*):(?P<portlocal>\d*) (?P<viplocal>\d{1,3}.\d{1,3}.\d{1,3}.\d{1,3}) (?P<hostremote>\w*):(?P<portremote>\d*) (?P<vipremote>\d{1,3}.\d{1,3}.\d{1,3}.\d{1,3})') 

class Interface(object):
	def __init__(self, line):
		m = re.match(lnx_pattern, line)
		if not m:
			error('Unable to match line: ' + line)
		
		# We're pretending to be the remote host! so 
		# when we bind we'll actually be oddly using the 
		# 'REMOTE' information
		self.host_local = m.group('hostlocal')
		self.port_local = int(m.group('portlocal'))	
		self.vip_local  = m.group('viplocal')
		
		self.host_remote = m.group('hostremote')
		self.port_remote = int(m.group('portremote'))
		self.vip_remote  = m.group('vipremote')

		self.sfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

	def bind(self):
		# We're pretending to be the remote host! so 
		# when we'll actually be oddly using the 
		# 'REMOTE' information	
		print 'Binding to host {} on port {}'.format(self.host_remote, self.port_remote)
		self.sfd.bind((self.host_remote, self.port_remote))

	def send(self, msg):
		self.sfd.sendto(default_ip_packet(msg), (self.host_local, self.port_local))
	
	def interface_print(self):
		print "{}:{} {} {}:{} {}".format(self.host_local, self.port_local, self.vip_local, self.host_remote, self.port_remote, self.vip_remote)

if __name__ == '__main__':	
	first_4 = 5
	last_4  = 10
	print pack_char(first_4, last_4)
	
