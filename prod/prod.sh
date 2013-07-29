#!/usr/bin/env zsh
export LD_LIBRARY_PATH=./lib
export PYTHONPATH=./lib/python/

ssd()
{
	start-stop-daemon -v $*
}

start()
{
	ssd -1 apertus0.stdout -2 apertus0.stderr -p apertus0.pid -m -b -S ./bin/apertus-server -- --port 0
	ssd -1 apertus1.stdout -2 apertus1.stderr -p apertus1.pid -m -b -S ./bin/apertus-server -- --port 0
	ssd -1 apertus2.stdout -2 apertus2.stderr -p apertus2.pid -m -b -S ./bin/apertus-server -- --port 0
	ssd -1 apertus-proxy.stdout -2 apertus-proxy.stderr -p apertus-proxy.pid -m -b -S ./bin/apertus-server -- --proxy --port 9899
	ssd -1 tropho.stdout -2 tropho.stderr -p tropho.pid -m -b -S ./bin/trophonius-server -- --port 23456 --control-port 23457
	spawn-fcgi -P meta.pid -d ./ -a 127.0.0.1 -p 9002 -- ./bin/meta-server --fcgi --trophonius-control-port 23457 --apertus-host 88.190.48.55 --apertus-port 9899
	sudo nginx -p ./www -c ../nginx.conf
}

stop()
{
	ssd -p apertus0.pid -K
	ssd -p apertus1.pid -K
	ssd -p apertus2.pid -K
	ssd -p apertus-proxy.pid -K
	ssd -p tropho.pid -K
	ssd -p meta.pid -K
	sudo nginx -p ./www -c ../nginx.conf -s stop
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
