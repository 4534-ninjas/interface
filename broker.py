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
		#self.re = re.compile(r'[^\xff]*(?:\xff})?\xff{([^\xff]*)\xff}(.*)')
		self.re = re.compile(r'([^\n]*)\n(.*)')
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
	print 'from {}: {}' % (peer, msg)
	x = {'from': peer, 'raw': msg}
	if msg[0] == '\x01':
		x['type'] = 'def'
	elif msg[0] == '\x02':
		x['type'] = 'msg'
	else:
		x['type'] = 'unknown'
	return json.dumps(x)

def unix_to_tcp(msg, sock):
	print 'from {}: {}' % (sock.getpeername(), msg)
	x = json.loads(msg)
	return x['raw']

def broadcast_tcp(msg):
	with tcp_lock:
		for s in tcp_clients:
			s.sendall(msg)

def broadcast_unix(msg):
	with unix_lock:
		for s in unix_clients:
			s.sendall(msg)

def update_rovers_present():
	with tcp_lock:
		def pack(sock):
			peer = sock.getpeername()
			return {'host': peer[0], 'port': peer[1]}
		rovers = map(pack, tcp_clients)
		print 'rovers present: '+str([s.getpeername() for s in tcp_clients])
		broadcast_unix(json.dumps({'type': 'rover_list', 'rovers': rovers}))

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

	def handle_timeout(self):
		self.on_done()

	def on_done(self):
		with tcp_lock:
			tcp_clients.remove(self.request)
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
			# maybe catch exceptions here & notify sender
			broadcast_tcp(unix_to_tcp(line, self.request))
		with unix_lock:
			unix_clients.add(self.request)
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
