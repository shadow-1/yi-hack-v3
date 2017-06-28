#!/bin/sh

CONF_FILE="/home/app/system.conf"

get_config()
{
	key=$1
	grep $1 $CONF_FILE | cut -d "=" -f2
}

if [ -f "/home/app/yi-hack-v3.7z" ]; then
        7za x /home/app/yi-hack-v3.7z
	rm /home/app/yi-hack-v3.7z
fi

if [[ $(get_config HTTPD) == "yes" ]] ; then
	lwsws -D
fi

if [[ $(get_config TELNETD) == "yes" ]] ; then
	telnetd
fi

if [[ $(get_config FTPD) == "yes" ]] ; then
	tcpsvd -vE 0.0.0.0 21 ftpd -w &
fi

if [ -f "/tmp/sd/startup.sh" ]; then
	/tmp/sd/startup.sh
fi
