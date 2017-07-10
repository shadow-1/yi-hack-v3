#!/bin/sh

ARCHIVE_FILE="/home/yi-hack-v3/yi-hack-v3.7z"
UDHCPC_SCRIPT_DEST="/home/app/script/default.script"
DHCP_SCRIPT_DEST="/home/app/script/wifidhcp.sh"
UDHCP_SCRIPT="/home/yi-hack-v3/default.script"
DHCP_SCRIPT="/home/yi-hack-v3/wifidhcp.sh"

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/yi-hack-v3/lib
export PATH=$PATH:/home/yi-hack-v3/bin

if [ -f $ARCHIVE_FILE ]; then
	7za x $ARCHIVE_FILE
	rm $ARCHIVE_FILE
fi

if [ -f /home/app/cloudAPI_real ]; then
	mv /home/app/cloudAPI /home/app/cloudAPI_real
	cp /home/yi-hack-v3/cloudAPI /home/app/
	rm $DHCP_SCRIPT_DEST
        rm $UDHCPC_SCRIPT_DEST
        cp $DHCP_SCRIPT $DHCP_SCRIPT_DEST
        cp $UDHCP_SCRIPT $UDHCPC_SCRIPT_DEST
fi
