#!/bin/sh

websocketd=/home/jpo/go/bin/websocketd
PORT=8080

cd "$(dirname "$0")"
exec $websocketd \
  --port=$PORT \
  --dir=websocket \
  --staticdir=static \
  --cgidir=cgi
