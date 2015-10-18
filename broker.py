#!/usr/bin/env python

import sys
import os
import time
import socket
import json
import signal
import SocketServer
import threading
import re
import struct
import base64

unix_addr = 'sock'
tcp_addr = ('0.0.0.0', 4321)

try:
	os.unlink(unix_addr)
except OSError:
	if os.path.exists(unix_addr):
		raise

tcp_clients = set()
tcp_lock = threading.Lock()

unix_clients = set()
unix_lock = threading.Lock()

class tcp_pkt_iter:
	def __init__(self, sock):
		#self.re = re.compile('[^\xff]*(?:\xff})?\xff{([^\xff]*)\xff}(.*)')
		self.re = re.compile('.*?\xff{([^\xff]*)\xff}(.*)', re.MULTILINE|re.DOTALL)
		#self.re = re.compile(r'([^\n]*)\n(.*)')
		self.sock = sock
		self.buf = ""

	def __iter__(self):
		return self

	def getmore(self):
		data = self.sock.recv(4096)
		print 'got: '+data.encode('string-escape')
		print 'buf: '+self.buf.encode('string-escape')
		if not data:
			raise StopIteration()
		self.buf += data

	def next(self):
		while not self.re.match(self.buf):
			self.getmore()
		match = self.re.match(self.buf)
		self.buf = match.group(2)
		return match.group(1)

def tcp_to_unix(msg, sock):
	msg = msg.replace('\\\\', '\\').replace('\\x', '\xff')
	peer = sock.getpeername()
	print 'from(tcp) %s: %s' % (peer, msg)
	x = {'from': peer, 'raw': base64.b64encode(msg)}
	if msg[0] == 'M':
		x['type'] = 'msg'
		(x['e'], x['n']) = struct.unpack('!II', msg[1:9])
		print 'e: %08x'%x['e']
		split = re.compile('([^\0]*)\0(.*)', re.MULTILINE|re.DOTALL)
		m = split.match(msg[9:])
		x['fmt'] = m.group(1)
		r = m.group(2)
		a = []
		try:
			for fmt in x['fmt']:
				if fmt == 'd':
					a.append(struct.unpack('!i', r[0:4])[0])
					r = r[4:]
				elif fmt == 'f':
					a.append(struct.unpack('!d', r[0:8])[0])
					r = r[8:]
				elif fmt == 's':
					m = split.match(r)
					a.append(m.group(1))
					r = m.group(2)
				else:
					raise TypeError('unknown args')
		except:
			return json.dumps({'type':'error', 'reason':'tcp unpacking', 'raw':base64.b64encode(msg)})+'\n'
		x['args'] = a

	elif msg[0] == 'D':
		x['type'] = 'dbg_def'
		(x['e'], x['n'], x['line'], x['enabled']) = struct.unpack('!III?', msg[1:14])
		print 'e: %08x'%x['e']
		m = re.match('([^\0]*)\0([^\0]*)\0([^\0]*)\0', msg[14:])
		x['file'] = m.group(1)
		x['fmt'] = m.group(2)
		x['sfmt'] = m.group(3)
	elif msg[0] == 'C':
		x['type'] = 'cmd_def'
		d = msg[1:].split('\n')
		if len(d) == 2:
			(x['op'], x['descr']) = d
		elif len(d) == 3:
			(x['op'], x['descr'], x['arg_descr']) = d
		else:
			return json.dumps({'type':'error', 'reason':'bad op descr fmt', 'raw':base64.b64encode(msg)})
	elif msg[0] == 'T':
		x['type'] = 'test_def'
		try:
			x['name'], x['descr'] = msg[1:].split('\n')
		except:
			return json.dumps({'type':'error', 'reason':'bad tst descr fmt', 'raw':base64.b64encode(msg)})
	elif msg[0] == 'B':
		x['type'] = 'bcast'
		broadcast_tcp_except(sock, msg[1:]+'\n')
	else:
		x['type'] = 'unknown'
	return json.dumps(x)+'\n'

def unix_to_tcp(msg, sock):
	print 'from(unix) %s: %s' % (sock.getpeername(), msg)
	x = json.loads(msg)
	if x['type'] == 'str':
		return x['msg']+'\n'
	elif x['type'] == 'b64':
		return base64.b64decode(x['msg'])+'\n'
	elif x['type'] == 'enumerate':
		return 'DBGS\nCMDS\n'
	elif x['type'] == 'cmd':
		try:
			return x['op']+' '+x['arg']+'\n'
		except:
			try:
				return x['op']+'\n'
			except:
				return json.dumps({'type':'error', 'reason':'cmd missing op/arg'})
	elif x['type'] == 'enable':
		try:
			return 'DBGE %08x\n' % x['e']
		except:
			return json.dumps({'type':'error', 'reason':'missing debug id'})
	elif x['type'] == 'disable':
		try:
			return 'DBGD %08x\n' % x['e']
		except:
			return json.dumps({'type':'error', 'reason':'missing debug id'})

	# XXX better way to handle this?
	raise ValueError('bad msg format, type='+str(x['type']))

def dst_rover_from_unix(msg):
	print 'extracting from '+msg
	x = json.loads(msg)
	try:
		h = str(x['rover']['host'])
		p = int(x['rover']['port'])
		return (h, p)
	except:
		return None

def broadcast_tcp_except(sock, msg):
	with tcp_lock:
		for s in tcp_clients:
			if s != sock:
				try:
					s.sendall(msg)
				except:
					tcp_clients.remove(s)
					s.close()

def broadcast_tcp(msg):
	broadcast_tcp_except(None, msg)

def broadcast_unix(msg):
	with unix_lock:
		for s in unix_clients:
		#	try:
				s.sendall(msg)
		#	except:
		#		unix_clients.remove(s)
		#		s.close()

def rover_lookup(rover):
	for sock in tcp_clients:
		peer = sock.getpeername()
		if peer[0] == rover[0] and peer[1] == rover[1]:
			return sock
	raise IndexError('no such rover')

def rovers_present():
	rovers = []
	with tcp_lock:
		for sock in tcp_clients:
			try:
				peer = sock.getpeername()
				rovers.append({'host': peer[0], 'port': peer[1]})
			except:
				# EBADF
				print 'dead client'
				pass
		#print 'rovers present: '+str([s.getpeername() for s in tcp_clients])
	return json.dumps({'type': 'rover_list', 'rovers': rovers})+'\n'

def broadcast_rovers_present():
	broadcast_unix(rovers_present())

class ThreadedTCPRequestHandler(SocketServer.BaseRequestHandler):
	def handle(self):
		print 'tcp connected'
		with tcp_lock:
			tcp_clients.add(self.request)
		broadcast_rovers_present()
		for pkt in iter(tcp_pkt_iter(self.request)):
			print 'got '+pkt
			broadcast_unix(tcp_to_unix(pkt, self.request))
		self.on_done()

	def handle_timeout(self):
		self.on_done()

	def handle_error(self):
		self.on_done()

	def on_done(self):
		self.request.close()
		with tcp_lock:
			tcp_clients.remove(self.request)
		broadcast_rovers_present()
		print 'tcp disconnected'

class ThreadedTCPServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
	pass

class ThreadedUnixStreamRequestHandler(SocketServer.BaseRequestHandler):
	def handle(self):
		print 'unix connected'
		with unix_lock:
			unix_clients.add(self.request)
		rp = rovers_present()
		print 'ROVERS PRESENT VVV'
		print rp
		self.request.sendall(rp)
		for line in iter(self.request.makefile().readline, ''):
			# XXX maybe catch exceptions here & notify sender?
			print 'got '+line
			try:
				msg = unix_to_tcp(line, self.request)
				dst_rover = dst_rover_from_unix(line)
				print 'dst_rover is '+json.dumps(dst_rover)
				if dst_rover is None:
					print 'broadcasting: '+msg
					broadcast_tcp(msg)
				else:
					try:
						print 'sending '+msg+' to '+dst_rover[0]+':'+str(dst_rover[1])
						with tcp_lock: # XXX WTF HALP
							rover_lookup(dst_rover).sendall(msg)
					except IndexError:
						print 'stale msg for rover at '+dst_rover[0]+':'+str(dst_rover[1])
					except:
						raise
			except ValueError:
				self.request.sendall(json.dumps({'type': 'error', 'reason': 'invalid message'})+'\n')
			except:
				raise
		self.on_done()

	def handle_timeout(self):
		self.on_done()

	def handle_error(self):
		self.on_done()

	def on_done(self):
		try:
			self.request.close()
		except:
			pass
		with unix_lock:
			try:
				unix_clients.remove(self.request)
			except:
				pass
		print 'unix disconnected'

class ThreadedUnixStreamServer(SocketServer.ThreadingMixIn, SocketServer.UnixStreamServer):
	pass

def pingloop():
	while True:
		broadcast_tcp('PING\n')
		time.sleep(0.75) # we expect a ping every 1s

if __name__ == "__main__":
	SocketServer.TCPServer.allow_reuse_address = True
	tcp_server = ThreadedTCPServer(tcp_addr, ThreadedTCPRequestHandler)
	ip, port = tcp_server.server_address
	print 'tcp: %s:%d' % (ip, port)
	tcp_server_thread = threading.Thread(target=tcp_server.serve_forever)
	tcp_server_thread.daemon = True
	tcp_server_thread.start()

	unix_server = ThreadedUnixStreamServer(unix_addr, ThreadedUnixStreamRequestHandler)
	path = unix_server.server_address
	print 'unix: %s' % path
	unix_server_thread = threading.Thread(target=unix_server.serve_forever)
	unix_server_thread.daemon = True
	unix_server_thread.start()

	pinger = threading.Thread(target=pingloop)
	pinger.daemon = True
	pinger.start()

	def sigh(a,b):
		#tcp_server.shutdown()
		#unix_server.shutdown()
		tcp_server.server_close()
		unix_server.server_close()
		os._exit(0)
	signal.signal(signal.SIGINT, sigh)

	signal.pause()
