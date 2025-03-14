#!/bin/sh

#check if fwupdmgr exists, redirect output to void, exit if fwupdmgr unavailable
if ! command -v fwupdmgr > /dev/null 2>&1; then
	exit 1
fi

devcount="$(fwupdmgr get-devices 2>/dev/null |grep -c 'Device ID')"
if [ "$devcount" != "0" ]; then
	sysname="$(cat /sys/class/dmi/id/product_name)"
	infos="$(fwupdmgr get-upgrades 2>/dev/null |grep -e "Current version" -e "New version" -e "└─" -e "Summary")"
	#printf "<%s>\n" "$infos"

	adevcount="$(echo "$infos" | grep -c '^└─')"

	printf "%s out of %s devices on %s have Firmware updates available (fwupdmgr)\n" "$adevcount" "$devcount" "$sysname"
	if [ "$adevcount" != "0" ]; then
		printf "\t"
		echo "$infos" | grep "^└─" |cut -c7-|tr '\n' ' '
		echo "$infos" | grep "^  └─" |cut -c9- |rev |cut -c2- |rev |tr -d '\n'
		printf " ["
		echo "$infos" | grep "Current version" |grep -Eo "[0-9\.]+" |tr -d '\n'
		printf " -> "
		echo "$infos" | grep "New version" |grep -Eo "[0-9\.]+" |tr -d '\n'
		printf "]\n"
	fi
fi
