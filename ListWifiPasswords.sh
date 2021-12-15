#!/bin/bash

get_ssid_list () {
netsh.exe wlan show profiles | grep ":" | cut -d':' -f2 | cut -c2-
}

get_password_from_ssid () {
#cat -A <<<"$1" #for debugging purposes to print ALL characters, especially invisible ones
netsh.exe wlan show profile name="$1" key=clear | grep 'Schl.sselinhalt' | cut -d':' -f2 | cut -c2-
}

get_ssid_list | tail -n +2 | while read ssid ; do
   ssid=${ssid%?} # trim newline
   passwd=$(get_password_from_ssid "$ssid") #call function and capture stdout to variable
   echo "$ssid -> $passwd"
done