import re
import sys
import socket

def quit():
	sys.exit(0)

def error(msg):
	print "Error: %s\n" % msg
	quit()


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
		self.port_remote = m.group('portremote')
		self.vip_remote  = m.group('vipremote')

		self.sfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

	def bind(self):
		# We're pretending to be the remote host! so 
		# when we'll actually be oddly using the 
		# 'REMOTE' information	
		self.sfd.bind((self.host_remote, self.port_remote))

	def send(self, msg):
		self.sfd.sendto(msg, (self.host_local, self.port_local))
	
	def interface_print(self):
		print "{}:{} {} {}:{} {}".format(self.host_local, self.port_local, self.vip_local, self.host_remote, self.port_remote, self.vip_remote)

	
