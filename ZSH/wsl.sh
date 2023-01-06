#!/bin/false
# shellcheck shell=bash
echo "Sourcing WSL $WSL_VERSION specifics"
export DISPLAY=localhost:0.0

function SetupRemoteX11 (){
	echo "On the remote server you might need to install the following and reboot: xauth"
	sudo apt install xauth
	xauth generate :0 . trusted
}

function SSHServer(){
	sudo service ssh --full-restart
}
alias StartSSH=SSHServer

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

function sshSync_int(){
	if [ ! -e "$1" ] || [ -z "$(ls "$1")" ]; then
		echo "ssh folder didn't exist on $1 or was empty"
		mkdir -p "$1"
		return
	fi
	for f in "$1"/*
	do
		otherfile="$2/$(basename "$f")"
		if [ ! -e "$otherfile" ] || [ "$f" -nt "$otherfile" ]; then
			#echo "$f exists in $1 but not in $2 or is newer in $1"
			cp "$f" "$otherfile"
		fi
	done
}

function sshSync(){
	sshSync_int "/mnt/c/Users/$WinUser/.ssh/" "$HOME/.ssh/"
	sshSync_int "$HOME/.ssh/" "/mnt/c/Users/$WinUser/.ssh/"
}
alias SyncSSH=sshSync
alias syncssh=sshSync
alias sshsync=sshSync
alias SSHSync=sshSync

function _netsh_retrieve_ssid_list () {
	netsh.exe wlan show profiles | grep ":" | cut -d':' -f2 | cut -c2-
}

function _netsh_get_password_for_known_ssid () {
	#cat -A <<<"$1" #for debugging purposes to print ALL characters, especially invisible ones
	netsh.exe wlan show profile name="$1" key=clear | grep 'Schlüsselinhalt\|Key Content' | cut -d':' -f2 | cut -c2-
}

function lswlanpw () {
	_netsh_retrieve_ssid_list | tail -n +2 | while read -r ssid ; do
		ssid=${ssid%?} # trim newline
		passwd=$(_netsh_get_password_for_known_ssid "$ssid") #call function and capture stdout to variable
		echo "$ssid -> $passwd"
	done
}

alias lswifipw=lswlanpw

function mountWinDrive(){
	local DriveLetter
	DriveLetter=$(echo "$1" | tr '[:upper:]' '[:lower:]')
	if [ ! "$DriveLetter" ]; then
		echo "Please provide Windows Drive Letter like: mountWinDrive E"
		return 255
	fi
	echo "attempting to mount Windows Drive $1:\\ to /mnt/$DriveLetter"
	#check if /mnt/DriveLetter doesn't exist or if it's empty, if there is something, abort with error
	if [ -d "/mnt/$DriveLetter" ]; then
		if [ "$(ls -A /mnt/"$DriveLetter")" ]; then
			echo "ERROR: cannot mount to /mnt/$DriveLetter: not empty"
			return 255
		fi
	else
		echo "creating directory /mnt/$DriveLetter"
		sudo mkdir "/mnt/$DriveLetter"
	fi
	sudo mount -t drvfs "$1": "/mnt/$DriveLetter"
	local retcode="$?"
	if [ "$retcode" -ne 0 ]; then
		echo "mount failed (code $retcode)"
		return $retcode
	fi

	echo "mounted the $1:\\ drive to /mnt/$DriveLetter"
}

#Windows shorthands
alias npp="\"/mnt/c/Program Files (x86)/Notepad++/Notepad++.exe\""

alias desk='/mnt/c/Users/$WinUser/Desktop'
alias winHome='/mnt/c/Users/$WinUser'
alias home=winHome
alias dt=desk
alias dl=down
