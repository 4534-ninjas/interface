#!/usr/bin/env python

import sys
import os
import time
import socket
import json
import signal
import SocketServer
import threading

unix_addr = '/tmp/sock'
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

def tcp_to_unix(line):
	return line

def unix_to_tcp(line):
	return line

class ThreadedTCPRequestHandler(SocketServer.BaseRequestHandler):
	def handle(self):
		with tcp_lock:
			tcp_clients.add(self.request)
		for line in iter(self.request.makefile().readline, ''):
			print line
			forward = tcp_to_unix(line)
			with unix_lock:
				for s in unix_clients:
					s.sendall(forward)
		with tcp_lock:
			tcp_clients.remove(self.request)
		print 'tcp disconnected'

class ThreadedTCPServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):
	pass

class ThreadedUnixStreamRequestHandler(SocketServer.BaseRequestHandler):
	def handle(self):
		with unix_lock:
			unix_clients.add(self.request)
		for line in iter(self.request.makefile().readline, ''):
			print line
			forward = unix_to_tcp(line)
			with tcp_lock:
				for s in tcp_clients:
					s.sendall(forward)
		with unix_lock:
			unix_clients.add(self.request)
		print 'unix disconnected'

class ThreadedUnixStreamServer(SocketServer.ThreadingMixIn, SocketServer.UnixStreamServer):
	pass

if __name__ == "__main__":
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

	signal.pause()

	tcp_server.shutdown()
	tcp_server.server_close()
	unix_server.shutdown()
	unix_server.server_close()
