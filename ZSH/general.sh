#!/bin/false
# shellcheck shell=bash
#i am using bash here since I need functions and path expansion which are in bash/zsh but not dash/sh or POSIX compatible
#but I set the shebang to /bin/false to prevent this being executed, this is meant to be sourced only

export EDITOR=nano
#the hash -d something=other is a bit like ~ means /mnt/user/home; ~something now means other
hash -d code="$CODE_LOC"
hash -d CODE="$CODE_LOC"

#script invocations
alias pack='$CODE_LOC/packAndDeleteFolder.sh'
alias unpack='$CODE_LOC/unpackAndDeleteArchive.sh'
alias search='$CODE_LOC/search.sh'
alias contentsearch='$CODE_LOC/contentsearch.sh'

#general shorthands
alias zshrc="nano ~/.zshrc"
alias cls=clear
alias gitlog="git log --branches --remotes --tags --graph --oneline --decorate --notes HEAD"
#"hash time(realtivetime) (HEAD -> master,...) email(name) subject"
#%h abbrev commit hash
#%an author name
#%ae author email
#%ar author date realtive
#%aI iso yyyy-mm-ddThh:mm:ss author date
#%ai iso-like format author date
#everywhere I used %a. it would have been possible to use %c. to get the committer instead of the author...
#%d ref names like --decorate of git log
#%s subjet
alias qqq=exit

function UpdateZSH (){
	git -C "$CODE_LOC" pull
	#compile all c files in the folder ZSH using the self-compile trick
	find "$CODE_LOC/ZSH" -type f -name "*.c" -exec sh -c '"$1"' _ {} \;
	find "$CODE_LOC/ZSH" -type f -name "*.cpp" -exec sh -c '"$1"' _ {} \;
	if [ -w ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme ]; then #if file exists and write permission is granted
		rm ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme
	fi
	echo "copying $CODE_LOC/ZSH/lukasaldersley.zsh-theme to  ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme"
	cp "$CODE_LOC/ZSH/lukasaldersley.zsh-theme" ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme
	omz reload
}

PROXY_ADDRESS_STRING="host:port"
export PROXY_ADDRESS_STRING
PROXY_USER="user"
export PROXY_USER

function SetUpAptProxyConfigFile(){
	if [ ! -w /etc/apt/apt.conf.d/proxy ]; then
		echo "apt proxy config file had no write permissions -> changing that"
		if [ ! -e /etc/apt/apt.conf.d/proxy ]; then
			echo "apt proxy config file didn't even exist -> creating now"
			sudo touch /etc/apt/apt.conf.d/proxy
		fi
		sudo chmod 664 /etc/apt/apt.conf.d/proxy
		sudo chgrp sudo /etc/apt/apt.conf.d/proxy
		ls -lash /etc/apt/apt.conf.d/proxy
	fi
}

function enableProxy(){
	SetUpAptProxyConfigFile
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
	 ~/repotools.elf SET "${2:-"$CODE_LOC"}" "${1:-"NONE"}" "${3:-"QUICK"}"
}

function lsRepo(){
	~/repotools.elf SHOW "${1:-"$(pwd)"}" "${2:-"QUICK"}"
}

alias lsgit=lsRepo
alias lsrepo=lsRepo
alias lsGit=lsRepo
alias lsgit=lsrepo
alias gitls=lsrepo

alias lsorigin="~/repotools.elf LIST ORIGINS"

function disableProxy(){
	unset {http,https,ftp,no}_proxy
	echo "">/etc/apt/apt.conf.d/proxy
	echo "disabled proxy"
}
