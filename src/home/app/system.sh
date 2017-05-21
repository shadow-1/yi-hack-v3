#!/bin/sh

CONF_FILE="/home/app/system.conf"

get_config()
{
	key=$1
	grep $1 $CONF_FILE | cut -d "=" -f2
}

if [[ $(get_config HTTPD) == "yes" ]] ; then
	lwsws -D
fi

if [[ $(get_config TELNETD) == "yes" ]] ; then
	telnetd
fi

if [[ $(get_config FTPD) == "yes" ]] ; then
	tcpsvd -vE 0.0.0.0 21 ftpd -w &
fi

/tmp/sd/startup.sh

