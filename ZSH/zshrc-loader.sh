#!/bin/false
# shellcheck shell=bash

## this script must be sourced in zshrc as follows (obviously adjust CODE_LOC):
## Ideally just use setup.sh to create a valid zshrc
##
### ST_SRC="/path/to/parent/folder/of/ShellTools"
### export ST_SRC
### . "$ST_SRC/ShellTools/zshrc-loader.sh"
##
##NOTE: in bash/ksh/zsh export CODE_LOC="/mnt/e/CODE would also be valid but not in POSIX where the assignement and export must be seperate"

#to forcibly log out another user su into their account and execute "DISPLAY=:1 gnome-session-quit --force"
#to see what users are running: "who"
#to forcibly log out a non-gui user: pkill -t "terminalDevice" such as sudo pkill -t pts/1

if [ ! -e "$ST_CFG" ]; then
	mkdir -p "$ST_CFG"
	echo "$ST_CFG directory didn't exist -> creating now"
fi

printf "\e[38;5;240mLoading Scripts from\e[0m %s\n" "$ST_SRC"
printf "last commit %s, %s local changes\n" "$(git -C "$ST_SRC" log --branches --remotes --tags --notes --pretty='[%C(brightblack)%as/%C(auto)%ar]%d' HEAD -1)" "$(git -C "$ST_SRC" status --ignore-submodules=dirty --porcelain=v2 -b --show-stash| grep -cv '# branch')"

PROXY_HOST=""
export PROXY_HOST
PROXY_USER=""
export PROXY_USER
PROXY_PORT=""
export PROXY_PORT

export EDITOR=nano

#script invocations
alias pack='$ST_SRC/archive.sh -p'
alias unpack='$ST_SRC/archive.sh -u'
alias search='$ST_SRC/search.sh -p'
alias contentsearch='$ST_SRC/search.sh -c -p'
alias sysupdate='$ST_SRC/AptTools.sh --full-update'
alias aptcheck='$ST_SRC/AptTools.sh --check'
alias aptstatus='$ST_SRC/AptTools.sh --unattended'
alias acat='$ST_CFG/assemblecat.elf'
alias cat-assemble='$ST_CFG/assemblecat.elf'
alias argtest='$ST_CFG/argtest.elf'
alias rm-recurse='$ST_CFG/rm_recurse.elf'

#custom git log
#e -> extended (name), x-> exact (author/committer)
alias   gitlog="git log --branches --remotes --tags --graph --notes --pretty=\"%C(auto)%h [%C(brightblack)%as/%C(auto)%ar]%d %C(brightblack)%ae:%C(auto) %s\" HEAD"
alias  egitlog="git log --branches --remotes --tags --graph --notes --pretty=\"%C(auto)%h [%C(brightblack)%as/%C(auto)%ar]%d %C(brightblack)%ae(%an):%C(auto) %s\" HEAD"
alias  xgitlog="git log --branches --remotes --tags --graph --notes --pretty=\"%C(auto)%h [%C(brightblack)%as|%cs/%C(auto)%ar|%cr]%d %C(brightblack)%ae|%ce:%C(auto) %s\" HEAD"
alias exgitlog="git log --branches --remotes --tags --graph --notes --pretty=\"%C(auto)%h [%C(brightblack)%as|%cs/%C(auto)%ar|%cr]%d %C(brightblack)%ae(%an)|%ce(%cn):%C(auto) %s\" HEAD"
alias procowners="sudo ps axo user:64 |sort|uniq"
alias gitupdate="git submodule foreach git pull --ff-only --prune ; git pull --ff-only --prune"
alias gitauthor='printf "Configured authors for %s:\n\t[System]: %s (%s)\n\t[Global]: %s (%s)\n\t[Local]: %s (%s)\n" "$(pwd)" "$(git config --system --get user.name)" "$(git config --system user.email)" "$(git config --global --get user.name)" "$(git config --global user.email)" "$(git config --local --get user.name)" "$(git config --local user.email)"'
#%C(auto) basically tells git to do colouring as it usually would in it's predefined versions until another colour instruction is given
#%C(string) basically means display a CSI defined 4-bit colour
#%h abbrev commit hash
#%as author date, short format (YYYY-MM-DD)
#%ar author date, relative
#%d ref names, like the --decorate option of git-log(1) [this is the indicator of what branches are where]
#%ae author email
#%s subject [ie commit message]
#everywhere I used %a. it would have been possible to use %c. to get the committer instead of the author... (as I have done in egitlog)

#general shorthands
alias l="ls --time-style=+\"%Y-%m-%d %H:%M:%S\" --color=tty -lash"
alias zshrc="nano ~/.zshrc"
alias cls=clear
alias qqq=exit
alias recrm=rm-recurse
alias ga="git add ."
alias gs="git status"
alias gse="gitauthor ; gs"
alias gl=gitlog
alias gu=gitupdate
alias cf=cleanFile
alias sf=sortFile
alias uz=UpdateZSH

searchapt(){
	printf "Apt packages:\n"
	apt-cache search "$1" | grep "$1"
	printf "\nInstallation status:\n"
	dpkg -l |grep "$1"
}

cleanFile(){
	___tempfile_local=$(mktemp)
	sort < "$1" | uniq > "$___tempfile_local"
	mv "$___tempfile_local" "$1"
}

sortFile(){
	___tempfile_local=$(mktemp)
	sort < "$1" > "$___tempfile_local"
	mv "$___tempfile_local" "$1"
}

UpdateZSH (){
	git -C "$ST_SRC" pull
	#compile all .c or .cpp files in the folders ZSH and C using the self-compile trick
	find "$ST_SRC/ZSH" "$ST_SRC/C" -type f -name "*.c" -exec sh -c '"$1"' _ {} \;
	find "$ST_SRC/ZSH" "$ST_SRC/C" -type f -name "*.cpp" -exec sh -c '"$1"' _ {} \;
	if [ -w ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme ]; then #if file exists and write permission is granted
		rm ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme
	fi
	echo "copying $ST_SRC/ZSH/lukasaldersley.zsh-theme to  ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme"
	cp "$ST_SRC/ZSH/lukasaldersley.zsh-theme" ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme
	omz reload
}

SetGitBase(){
	"$ST_CFG/repotools.elf" --set -n"${1:-"NONE"}" "${2:?}" "${3:-"-q"}"
}
alias sgb=SetGitBase

lsRepo(){
	"$ST_CFG/repotools.elf" --show "${1:-"$(pwd)"}" "${2:-"-q"}"
}

compare(){
	#EachWidth="$(bc <<< "$COLUMNS/2")"
	#diff --width="$EachWidth" -y --color --report-identical-files --suppress-common-lines <(cat "$1") <(cat "$2")
	code --diff "$1" "$2"
}

alias lsgit=lsRepo
alias lsrepo=lsRepo
alias lsGit=lsRepo
alias lsgit=lsrepo
alias gitls=lsrepo
alias lsorigin='$ST_CFG/repotools.elf --list'


SetUpAptProxyConfigFile(){
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

HasLocalProxyServer(){
	#return default false if it's not overridden in other scripts
	#NOTE: it WILL be overriden in custom non-public extensions
	echo "default false for Local Proxy"
	return 1
}

enableProxy(){
	if [ -n "$PROXY_HOST" ] ; then
		SetUpAptProxyConfigFile

		if HasLocalProxyServer ; then
			#echo "woking with local proxy on $PROXY_HOST, port $PROXY_PORT"
			#PROXY_NAME will be set in a nonpublic extension
			echo "detected $PROXY_NAME on port $PROXY_PORT"
			PROXY_STRING="http://$PROXY_HOST:$PROXY_PORT"
			export {http,https,ftp}_proxy="$PROXY_STRING"
			export {HTTP,HTTPS,FTP}_PROXY="$PROXY_STRING"
			printf 'Acquire {\n  HTTP::proxy "%s";\n  HTTPS::proxy "%s";\n}\n' "$PROXY_STRING" "$PROXY_STRING" > /etc/apt/apt.conf.d/proxy
			unset PROXY_STRING
		else
			ProxHost="${PROXY_HOST}${PROXY_PORT:+":$PROXY_PORT"} (${PROXY_NAME:-"Proxy"})"
			if [ -z "$PROXY_USER" ]; then
				read -r "?Please enter the Username for $ProxHost" PROXY_USER
				ProxHost="$PROXY_USER@$ProxHost"
			fi
			local PROXY_PW
			#please note, read is a shell builtin. in bash it would be -r -s -p "prompt" TargetVar while in ZSH it is -r -s "?prompt" TargetVar
			#-r is for "raw" mode; basically disable \'s special treatment
			#-s is for silent -> don't print the password back out to the shell
			#in bash -p is for prompt, to specify a desired prompt text, in zsh it's the first parameter, if that parameter starts with ?
			read -r -s "?Please Enter Password for ${ProxHost}: " PROXY_PW
			unset ProxHost
			PROXY_STRING="http://${PROXY_USER}:${PROXY_PW}@${PROXY_HOST}${PROXY_PORT:+":$PROXY_PORT"}"
			#${PARAM:+WORD} means: if PARAM is null or unset -> "", if PARAM is set then substitute expansion of WORD
			#In this case that means: if PROXY_PORT isn't null, add ':' followed by the port number otherwise add nothing
			#echo -e "\n<$PROXY_STRING>"
			export {http,https,ftp}_proxy="$PROXY_STRING"
			export {HTTP,HTTPS,FTP}_PROXY="$PROXY_STRING"
			echo -e "\n<$HTTP_PROXY>"
			printf 'Acquire {\n  HTTP::proxy "%s";\n  HTTPS::proxy "%s";\n}\n' "$PROXY_STRING" "$PROXY_STRING" > /etc/apt/apt.conf.d/proxy
			unset PROXY_STRING
			PROXY_PW=""
			echo ""
		fi
		export {no_proxy,NO_PROXY}="127.0.0.1,localhost,::1"

		if [ -n "$1" ]; then #arg 1 is nonzero
			printf "supplied parameter \"%s\" -> skipping connection check\n" "$1"
			return
		fi

		#NOTE: the test if proxy auth is working is not functioning right now, it always accepts
		#it works if the proxy hasn't yet been enabled since System boot, otherwise it caches and always succeeds
		local ConnTestURL='www.google.de' # I loathe google, but it's highly likely they'll be up and not blocked by a firewall, so I'll use them as a connection test
		curl --silent --max-time 1 $ConnTestURL > /dev/null
		local CurlRes=$?
		if [ 0 -eq $CurlRes ]; then
			printf "enabled proxy (%s)\n" "$PROXY_NAME"
		elif [ 5 -eq $CurlRes ]; then
			printf "cannot resolve proxy host (%s) for %s, tentatively leaving proxy enabled, but might not work" "$PROXY_HOST" "$PROXY_NAME"
		elif [ 6 -eq $CurlRes ]; then
			printf "cannot resolve test host (%s) for %s, tentatively leaving proxy enabled, but might not work" "$ConnTestURL" "$PROXY_NAME"
		else
			#no connectiopn
			printf "Connection to %s not possible (likely wrong password)\n" "$PROXY_NAME"
			disableProxy
		fi
	else
		echo "PROXY_HOST not provided -> NO ACTION"
	fi
}

disableProxy(){
	unset {http,https,ftp,no}_proxy
	echo "">/etc/apt/apt.conf.d/proxy
	echo "disabled proxy"
}

sysver(){
	#the following line parses /etc/os-release for the pretty_name and the version and kind of concatenates them. The pretty name usually contains a shorter version, so assemblecat is used which omits the common parts
	#generally pretty_name will be the topmost entry and version will be somewhere down the line, with version_id below that. Alpine doesn't have version, just version_id, therefore I had to also allow version_id, but due to the always-greedy setup of sed
	#I would always capture version_id which doesn't contain the version codename. Furthermore in alpine pretty_name is BELOW version_id, so I need to sort the input to be able to regex it. then, to avoid always matching version_id, I reverse sort it,
	#this makes the first greedy match contain version_id as long as there is version, version_id therefore only is a fallback (I had to flip the capture groups in the regex as well)
	#--equal=\" v\" tells assemblecat to treat ' ' and 'v' the same to allow assembling of "Alpine Linux v3.17" with " 3.17.0" into "Alpine Linux v3.17.0". I wouldn't need the equality list if there wasn#t a space in the second string
	#however most distros need that space to avoid "Kali GNU/Linux Rolling2023.3"
	eval "$ST_CFG/assemblecat.elf --equal=\" v\" $(sort -r < /etc/os-release | tr -d '\n' | sed -nE 's~.*VERSION(_ID)?="?([-a-zA-Z0-9\._ \(\)/]+)"?.*PRETTY_NAME="([-a-zA-Z0-9\._ \(\)/]+)".*~"\3" " \2"~p')" |tr -d '\n'
	printf " [%s %s %s]\n" "$(uname --operating-system)" "$(uname --kernel-release)" "$(uname --machine)" #machine is x86-64 or arm or something
}

shellver(){
	printf "\e[38;5;240mRunning Scripts from \e[0m%s\n" "$ST_SRC"
	if [ -e "${ST_SRC}/../ShellToolsExtensionLoader.sh" ]; then
		printf "\e[38;5;240mwith Extensions from \e[0m%s\n" "$(realpath "$(dirname "${ST_SRC}/../ShellToolsExtensionLoader.sh")")"
	fi
}

IsWSL=0
#use cmd.exe to get windows username, discard cmd's stderr (usually because cmd prints a warning if this was called from somewhere within the linux filesystem), then strip CRLF from the result
WinUser="$(cmd.exe /c "echo %USERNAME%" 2>/dev/null | tr -d '\n\r')"
export WinUser
#the reason I don't just check $WSL_DISTRO_NAME is because I want to know if it's WSL1 or WSL2. there may be differences I need to address at some point
#this is a command that's EXPECTED to fail every time this is run on a non-WSL installation, so the stderr redirect and masking of the return code are intentional
#iconv is a tool to convert character encodings. this is needed to change windows's UTF16LE to the UTF-8 I want
{ WSL_VERSION="$(/mnt/c/Windows/System32/wsl.exe -l -v | iconv -f unicode|grep -E "\b$WSL_DISTRO_NAME\s+Running" | awk '{print $NF}' | cut -c-1)"; } 2> /dev/null || true
if [ -n "$WSL_VERSION" ]; then
	IsWSL=1
fi

export IsWSL

if [ $IsWSL -eq 1 ]; then
	#shellcheck source=WSL.sh
	. "$ST_SRC/ZSH/WSL.sh"
else
#check if this is debian-esque, if it's not, it's not supported, exit immediatley
	#shellcheck source=BARE.sh
	. "$ST_SRC/ZSH/BARE.sh"
fi

if [ -e "$ST_SRC/../ShellToolsExtensionLoader.sh" ]; then
	echo "Loading Non-Public Extensions"
	# shellcheck disable=SC1091 # reason: this file may very well not exist, as it will only exist for some systems where there are non-public ShellTools Extensions.
	# shellcheck disable=SC1090 # shellcheck can't find the file, obviously
	. "$ST_SRC/../ShellToolsExtensionLoader.sh"
fi

unset IsWSL


printf "done loading utility scripts\n\n"
printf "Welcome to "
sysver

if [ "$WSL_VERSION" -ne 1 ]; then
	#system load is hardcoded in WSL as per https://github.com/microsoft/WSL/issues/945 and allegedly fixed in wsl2 as per https://github.com/microsoft/WSL/issues/2814
	printf "System load averages (1/5/15 min) are %s out of %s\n" "$(cut -f1-3 -d ' ' /proc/loadavg | tr ' ' '/')" "$(grep -c ^processor /proc/cpuinfo).0"
fi

# shellcheck disable=SC2046 #reason: I want wordsplitting to happen on the date
printf "System up since %s (%s)\n" "$(uptime --since)" "$("$ST_CFG/timer-zsh.elf" STOPHUMAN $(uptime --since |tr ':-' ' '))"

"$ST_SRC/AptTools.sh" --unattended

devcount="$(fwupdmgr get-devices 2>/dev/null |grep -c 'Device ID')"
if [ "$devcount" != "0" ]; then
	sysname="$(cat /sys/class/dmi/id/product_name)"
	infos="$(fwupdmgr get-upgrades 2>/dev/null |grep -e "Current version" -e "New version" -e "└─" -e "Summary")"
	#printf "<%s>\n" "$infos"

	adevcount="$(grep -c '^└─' <<< "$infos")"

	if [ "$adevcount" != "0" ]; then
		printf "%s out of %s devices on %s have Firmware updates available (updmgr\n" "$adevcount" "$devcount" "$sysname"
		printf "\t"
		grep "^└─" <<< "$infos" |cut -c7-|tr '\n' ' '
		grep "^  └─" <<< "$infos" |cut -c9- |rev |cut -c2- |rev |tr -d '\n'
		printf " ["
		grep "Current version" <<< "$infos" |grep -Eo "[0-9\.]+" |tr -d '\n'
		printf " -> "
		grep "New version" <<< "$infos" |grep -Eo "[0-9\.]+" |tr -d '\n'
		printf "]\n"
	fi
fi
