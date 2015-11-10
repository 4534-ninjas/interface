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
		self.re = re.compile(r'[^\xff]*(?:\xff})?\xff{([^\xff]*)\xff}(.*)')
		#self.re = re.compile(r'([^\n]*)\n(.*)')
		self.sock = sock
		self.buf = ""

	def __iter__(self):
		return self

	def getmore(self):
		data = self.sock.recv(4096)
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
	peer = sock.getpeername()
	print 'from %s: %s' % (peer, msg)
	x = {'from': peer, 'raw': base64.b64encode(msg)}
	if msg[0] == 'D':
		x['type'] = 'def'
	elif msg[0] == 'M':
		x['type'] = 'msg'
	else:
		x['type'] = 'unknown'
	return json.dumps(x)+'\n'

def unix_to_tcp(msg, sock):
	print 'from %s: %s' % (sock.getpeername(), msg)
	x = json.loads(msg)
	if x['type'] == 'raw':
		p = base64.b64decode(x['raw'])
	elif x['type'] == 'enumerate':
		p = '?'
	elif x['type'] == 'enable':
		p = '+'+'XXX'
	elif x['type'] == 'disable':
		p = '-'+'XXX'
	else:
		# XXX better way to handle this?
		raise ValueError()
	return '\xff{'+p.replace('\\','\\\\').replace('\xff','\\x')+'\xff}'

def broadcast_tcp(msg):
	with tcp_lock:
		for s in tcp_clients:
			try:
				s.sendall(msg)
			except:
				tcp_clients.remove(s)
				s.close()

def broadcast_unix(msg):
	with unix_lock:
		for s in unix_clients:
			try:
				s.sendall(msg)
			except:
				unix_clients.remove(s)
				s.close()

def update_rovers_present():
	with tcp_lock:
		def pack(sock):
			peer = sock.getpeername()
			return {'host': peer[0], 'port': peer[1]}
		rovers = map(pack, tcp_clients)
		#print 'rovers present: '+str([s.getpeername() for s in tcp_clients])
		broadcast_unix(json.dumps({'type': 'rover_list', 'rovers': rovers})+'\n')

class ThreadedTCPRequestHandler(SocketServer.BaseRequestHandler):
	def handle(self):
		print 'tcp connected'
		with tcp_lock:
			tcp_clients.add(self.request)
		update_rovers_present()
		for pkt in iter(tcp_pkt_iter(self.request)):
			broadcast_unix(tcp_to_unix(pkt, self.request))
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
		with tcp_lock:
			try:
				tcp_clients.remove(self.request)
			except:
				pass
		update_rovers_present()
		print 'tcp disconnected'

class ThreadedTCPServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
	pass

class ThreadedUnixStreamRequestHandler(SocketServer.BaseRequestHandler):
	def handle(self):
		print 'unix connected'
		with unix_lock:
			unix_clients.add(self.request)
		for line in iter(self.request.makefile().readline, ''):
			# XXX maybe catch exceptions here & notify sender?
			try:
				broadcast_tcp(unix_to_tcp(line, self.request))
			except ValueError:
				try:
					self.request.sendall(json.dumps({'type': 'error', 'reason': 'invalid message'})+'\n')
				except:
					break
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

	def sigh(a,b):
		#tcp_server.shutdown()
		#unix_server.shutdown()
		tcp_server.server_close()
		unix_server.server_close()
		os._exit(0)
	signal.signal(signal.SIGINT, sigh)

	signal.pause()
