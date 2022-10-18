#!/bin/false
# shellcheck shell=bash
echo "Sourcing WSL $WSL_VERSION specifics"
export DISPLAY=localhost:0.0

function SetupRemoteX11 (){
	echo "On the remote server you might need to install the following and reboot: xauth"
	sudo apt install xauth
	xauth generate :0 . trusted
}

{ WinUser=$(cmd.exe /c "echo %USERNAME%"|rev|cut -c2-|rev); } 2> /dev/null
export WinUser
#the whole crap about |rev|cut -c2-|rev is to remove the \r\n Windows' echo adds
#the reason for the double rev is that cut only has functionality to return everything between two markers of from a marker to the end of line or to a fixed index.
#Since the username can be any length and I always want to remove the last two chars I need to reverse the string, and keep everything from index 2 up to
#the end of line, so that when I reverse it again to get back the original order I actually have everything from the beginning and have removed only the last two.
#Also Windows Prints the following (I marked it with double hashes) to stderr, but it works anyway
#the grouping by using {} and 2> /dev/null just redirects stderr for this line to null since I don't need this particular error
##"\\wsl$\Ubuntu-20.04\home\master"
##CMD.EXE wurde mit dem oben angegebenen Pfad als aktuellem Verzeichnis gestartet.
##UNC-Pfade werden nicht unterstützt.
##Stattdessen wird das Windows-Verzeichnis als aktuelles Verzeichnis gesetzt.
##echo "$WinUser"

alias shutdown='{ /mnt/c/Windows/System32/cmd.exe /C shutdown /s /t 00 } 2> /dev/null'
alias reboot='{ /mnt/c/Windows/System32/cmd.exe /C shutdown /r /t 00 } 2> /dev/null'
alias hibernate='{ /mnt/c/Windows/System32/cmd.exe /C shutdown /h } 2> /dev/null'
alias sshSync='rm -rf "/mnt/c/Users/$WinUser/.ssh"; cp -r ~/.ssh "/mnt/c/Users/$WinUser/.ssh"'

_netsh_retrieve_ssid_list () {
	netsh.exe wlan show profiles | grep ":" | cut -d':' -f2 | cut -c2-
}

_netsh_get_password_for_known_ssid () {
	#cat -A <<<"$1" #for debugging purposes to print ALL characters, especially invisible ones
	netsh.exe wlan show profile name="$1" key=clear | grep 'Schlüsselinhalt' | cut -d':' -f2 | cut -c2-
}

lswlanpw () {
	_netsh_retrieve_ssid_list | tail -n +2 | while read -r ssid ; do
		ssid=${ssid%?} # trim newline
		passwd=$(_netsh_get_password_for_known_ssid "$ssid") #call function and capture stdout to variable
		echo "$ssid -> $passwd"
	done
}

alias lswifipw=lswlanpw

#Windows shorthands
alias npp="\"/mnt/c/Program Files (x86)/Notepad++/Notepad++.exe\""

alias desk='/mnt/c/Users/$WinUser/Desktop'
alias winHome='/mnt/c/Users/$WinUser'
alias home=winHome
alias dt=desk
alias dl=down