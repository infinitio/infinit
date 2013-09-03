#!/usr/bin/env zsh
export LD_LIBRARY_PATH=./lib
export PYTHONPATH=./lib/python/
INSTALL_DIR=$PWD
RUNTIME_DIR=$PWD/runtime

APERTUS_PORT=9900
TROPHO_PORT=23458
TROPHO_CPORT=23459
META_FCGI_PORT=9003

ssd()
{
	mkdir -p $RUNTIME_DIR
	/sbin/start-stop-daemon -v -d $RUNTIME_DIR $*
}

start()
{
	ssd -b -p $RUNTIME_DIR/apertus0.pid -m -S -x $INSTALL_DIR/bin/apertus-server -- --port 0
	ssd -b -p $RUNTIME_DIR/apertus1.pid -m -S -x $INSTALL_DIR/bin/apertus-server -- --port 0
	ssd -b -p $RUNTIME_DIR/apertus2.pid -m -S -x $INSTALL_DIR/bin/apertus-server -- --port 0
	ssd -b -p $RUNTIME_DIR/apertus-proxy.pid -m -S -x $INSTALL_DIR/bin/apertus-server -- --proxy --port $APERTUS_PORT
	ssd -b -p $RUNTIME_DIR/tropho.pid -m -S -x $INSTALL_DIR/bin/trophonius-server -- --port $TROPHO_PORT --control-port $TROPHO_CPORT
	echo spawn-fcgi -P meta.pid -d ./ -a 127.0.0.1 -p $META_FCGI_PORT -- ./bin/meta-server --fcgi --trophonius-control-port $TROPHO_CPORT --apertus-host 88.190.48.55 --apertus-port $APERTUS_PORT
	echo sudo nginx -p ./www -c ../nginx.conf
}

stop()
{
	ssd -p $RUNTIME_DIR/apertus0.pid -K
	ssd -p $RUNTIME_DIR/apertus1.pid -K
	ssd -p $RUNTIME_DIR/apertus2.pid -K
	ssd -p $RUNTIME_DIR/apertus-proxy.pid -K
	ssd -p $RUNTIME_DIR/tropho.pid -K
	ssd -p $RUNTIME_DIR/meta.pid -K
	echo sudo nginx -p ./www -c ../nginx.conf -s stop
}


usage()
{
	echo "./start_prod.sh [start|stop]"
}

case $1 in
	start)
		start
	;;
	stop)
		stop
	;;
	*)
		usage
	;;
esac
