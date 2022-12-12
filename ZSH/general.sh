#!/bin/false
# shellcheck shell=bash
#i am using bash here since I need functions and path expansion which are in bash/zsh but not dash/sh or POSIX compatible
#but I set the shebang to /bin/false to prevent this being executed, this is meant to be sourced only

export EDITOR=nano
#the hash -d something=other is a bit like ~ means /mnt/user/home; ~something now means other
hash -d CODE="$CODE_LOC"
hash -d code="$CODE_LOC"

#script invocations
alias pack='$CODE_LOC/packAndDeleteFolder.sh'
alias unpack='$CODE_LOC/unpackAndDeleteArchive.sh'
alias search='$CODE_LOC/search.sh'
alias contentsearch='$CODE_LOC/contentsearch.sh'

#general shorthands
alias zshrc="nano ~/.zshrc"
alias cls=clear
alias gitlog="git log --branches --remotes --tags --graph --oneline --decorate --notes HEAD"
alias qqq=exit
alias dotnet=~/.dotnet/dotnet

function UpdateZSH (){
	pushd "$CODE_LOC" || return
	git pull
	#compile all c files in the folder ZSH using the self-compile trick
	find "$CODE_LOC/ZSH" -type f -name "*.c" -exec sh -c '"$1"' _ {} \;
	if [ -w ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme ]; then #if file exists and write permission is granted
		rm ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme
	fi
	cp "$CODE_LOC/ZSH/lukasaldersley.zsh-theme" ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme
	popd || return
	omz reload
}

PROXY_ADDRESS_STRING="host:port"
export PROXY_ADDRESS_STRING
PROXY_USER="user"
export PROXY_USER

function enableProxy(){
	local PROXY_PW
	#please note, read is a shell builtin. in bash it would be -r -s -p "prompt" TargetVar while in ZSH it is -r -s "?prompt" TargetVar
	read -r -s "?Please Enter Password for Proxy ($PROXY_ADDRESS_STRING, user $PROXY_USER):" PROXY_PW
	export {http,https,ftp}_proxy="http://$PROXY_USER:$PROXY_PW@$PROXY_ADDRESS_STRING"
	printf 'Acquire {\n  HTTP::proxy "http://%s:%s@%s";\n  HTTPS::proxy "http://%s:%s@%s";\n}\n' "$PROXY_USER" "$PROXY_PW" "$PROXY_ADDRESS_STRING" "$PROXY_USER" "$PROXY_PW" "$PROXY_ADDRESS_STRING">/etc/apt/apt.conf.d/proxy
	PROXY_PW=""
	echo ""
	#NOTE: the test if proxy auth is working is not functioning right now, it always accepts
	local ConnTestURL='www.google.de'
	if curl --silent --max-time 1 $ConnTestURL > /dev/null
	then
		echo "enabled proxy"
	else
		#no connectiopn
		echo "Connection not possible (likely wrong password)"
		disableProxy
	fi
}

function SetGitBase(){
	if [ -z "$1" ]; then
		echo "You MUST supply th NAME of the desired repo acces type (currently none, internal, local, global)"
		echo "such as 'SetGitBase general' or 'SetGitBase none /path/to/repo'"
		return
	fi
	ActionBase="${2:-"$CODE_LOC"}"
	echo "setting $ActionBase and all submodules to $1"
	pushd "$ActionBase" || exit
	git submodule status --recursive | sed -n 's|.[0-9a-fA-F]\+ \([^ ]\+\)\( .*\)\?|\1|p' | ~/repotools.elf "$ActionBase" SET "$1"
	popd || return
}

function lsRepo(){
	ActionBase="${1:-"$(pwd)"}"
	pushd "$ActionBase" || exit
	~/repotools.elf "$ActionBase" SHOW
	popd || return
}

function disableProxy(){
	unset {http,https,ftp,no}_proxy
	echo "">/etc/apt/apt.conf.d/proxy
	echo "disabled proxy"
}