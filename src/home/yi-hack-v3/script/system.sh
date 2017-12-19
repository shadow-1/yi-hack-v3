#!/bin/sh

CONF_FILE="/yi-hack-v3/etc/system.conf"

if [ -d "/usr/yi-hack-v3" ]; then
        YI_HACK_V3_PREFIX="/usr"
elif [ -d "/home/yi-hack-v3" ]; then
        YI_HACK_V3_PREFIX="/home"
fi

get_config()
{
        key=$1
        grep $1 $YI_HACK_V3_PREFIX$CONF_FILE | cut -d "=" -f2
}

if [ -d "/usr/yi-hack-v3" ]; then
	export LD_LIBRARY_PATH=/home/libusr:$LD_LIBRARY_PATH:/usr/yi-hack-v3/lib
	export PATH=$PATH:/usr/yi-hack-v3/bin
elif [ -d "/home/yi-hack-v3" ]; then
	export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/lib:/home/yi-hack-v3/lib
	export PATH=$PATH:/home/base/tools:/home/yi-hack-v3/bin
fi

hostname -F $YI_HACK_V3_PREFIX/yi-hack-v3/etc/hostname

if [[ $(get_config HTTPD) == "yes" ]] ; then
	lwsws -D
fi

if [[ $(get_config TELNETD) == "yes" ]] ; then
	telnetd
fi

if [[ $(get_config FTPD) == "yes" ]] ; then
	tcpsvd -vE 0.0.0.0 21 ftpd -w &
fi

if [[ $(get_config DROPBEAR) == "yes" ]] ; then
        dropbear
fi

if [ -f "/tmp/sd/startup.sh" ]; then
	/tmp/sd/startup.sh
elif [ -f "/home/hd1/startup.sh" ]; then
	/home/hd1/startup.sh
fi
