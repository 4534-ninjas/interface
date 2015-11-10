#!/bin/sh

websocketd=/home/jpo/go/bin/websocketd
PORT=8080

if [ "X$1" = Xdev ]; then
	static='--devconsole'
else
	static='--staticdir=static --cgidir=cgi'
fi

cd "$(dirname "$0")"
exec $websocketd \
  --port=$PORT \
  $static \
  --dir=websocket
