#!/bin/sh

CONF_FILE="/home/yi-hack-v3/etc/system.conf"

get_config()
{
        key=$1
        grep $1 $CONF_FILE | cut -d "=" -f2
}

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/lib:/home/yi-hack-v3/lib
export PATH=$PATH:/home/base/tools:/home/yi-hack-v3/bin

hostname -F /home/yi-hack-v3/etc/hostname

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
