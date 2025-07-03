#!/bin/false
# shellcheck shell=bash
#I am aware this is zsh not bash, but shellcheck doesn't support that
#usually I would limit myself to POSIX compliant code (=sh) but here I need to deviate from that as zsh's read is similar to bash's but different from POSIX/sh's and if I do that I may as well use features not in sh such as here-strings and brace-expansion

#NOTE: if I clobbered something with an alias 'type -a *whateverIsuspectIsClobbered*' will give me a list of every possible meaning

## this script must be sourced in zshrc as follows (obviously adjust ST_SRC and ST_CFG):
## ST_CFG can be anything as long as it's writeable by ShellTools, below is just a suggestion
##
### ST_SRC="/path/to/ShellTools"
### ST_CFG="$HOME/.shelltools"
### export ST_SRC
### export ST_CFG
### . "$ST_SRC/ZSH/zshrc-loader.sh"
##
##NOTE: in bash/ksh/zsh 'export ST_CFG="$HOME/.shelltools"' would also be valid but not in POSIX where the assignement and export must be seperate"

#to forcibly log out another user su into their account and execute "DISPLAY=:1 gnome-session-quit --force"
#to see what users are running: "who"
#to forcibly log out a non-gui user: pkill -t "terminalDevice" such as sudo pkill -t pts/1

## To run a script with the users default shell but still have a fallback shebang use the following
# # Prevent infinite loops using an environment flag
# if [ -z "$IsOnDefaultShell" ]; then
# 	export IsOnDefaultShell=1
# 	exec "${SHELL:-/bin/sh}" "$0" "$@"
# fi
# unset IsOnDefaultShell
# echo "Running in: $(which "$(ps -p $$ -o comm=)")"

#shellcheck disable=SC2120 # reason: The parameter is optional, therefore it needn't be used. it's only use is in an alias for quicker development
ST_CoreUpdate (){
	printf "Updating/Installing ShellTools on branch %s" "$(git -C "$ST_SRC" symbolic-ref --short HEAD)"
	BranchAdvance=0
	if [ "${1:-"--pull"}" != "--nopull" ]; then
		echo ""
		if [ "${ST_EXTENSION_DOES_UPDATE:-0}" -ne 1 ] && [ -e "$ST_SRC/../ShellToolsExtensionLoader.sh" ] && [ "$(git -C "$(realpath "$ST_SRC/../")" rev-parse --show-toplevel 2>/dev/null)" != "$(git -C "$ST_SRC" rev-parse --show-toplevel 2>/dev/null)" ]; then
			#don't just randomly go around updating foreign git repositories without explicit user input
			#However if ST_EXTENSION_DOES_UPDATE is set, the currently loaded extension has an update routine which is run in a modified UpdateShellTools. In that case, suppress this warning
			printf "ShellToolsExtensionLoader.sh exists (at %s) -> there possibly is a parent repo, if so please update that MANUALLY\nShellTools will NOT automatically update anything other than itself\n" "$(realpath "$ST_SRC/../")"
		fi
		if ! git -C "$ST_SRC" symbolic-ref --short HEAD 1>/dev/null 2>&1 ; then
			#if I am on any branch, stay there, if I'm on a tag or a detached commit, go to master
			printf "INFO: ShellTools was on %s, checking out master\n" "$(git -C  "$ST_SRC" symbolic-ref --short HEAD 2>/dev/null || git -C "$ST_SRC" describe --tags --exact-match HEAD 2>/dev/null || git -C "$ST_SRC" rev-parse --short HEAD)"
			git -C "$ST_SRC" checkout master
		fi
		if git -C "$ST_SRC" pull --ff-only ; then
			BranchAdvance=1
			date +%s > "$ST_CFG/.lastfetch"
		elif git -C "$ST_SRC" merge origin/"$(git -C  "$ST_SRC" symbolic-ref --short HEAD)" --ff-only; then
			#I can't pull, per se, but I CAN ff-merge (ie I currently don't have network but I do have the stuff cached)
			BranchAdvance=1
		else
			printf "\e[38;5;9mAutomatic git pull --ff-only failed, please check manually\e[0m\n"
		fi
	else
		echo " without pulling git"
	fi
	#compile all .c or .cpp files in the folders ZSH and C using the self-compile trick
	find "$ST_SRC/ZSH" "$ST_SRC/C" -type f -name "*.c" -exec sh -c '"$1"' _ {} \;
	find "$ST_SRC/ZSH" "$ST_SRC/C" -type f -name "*.cpp" -exec sh -c '"$1"' _ {} \;
	if [ -w "$ZSH/custom/themes/" ]; then #if file exists and write permission is granted
		if [ -e "$ZSH/custon/themes/lukasaldersley.zsh-theme" ]; then
			rm "$ZSH/custom/themes/lukasaldersley.zsh-theme"
		fi
		echo "copying $ST_SRC/ZSH/lukasaldersley.zsh-theme to  $ZSH/custom/themes/lukasaldersley.zsh-theme"
		cp "$ST_SRC/ZSH/lukasaldersley.zsh-theme" "$ZSH/custom/themes/lukasaldersley.zsh-theme"
	else
		echo "WARNING: cannot update $ZSH/custom/themes/lukasaldersley.zsh-theme: not writeable"
	fi
	if [ "$BranchAdvance" -eq 1 ] || [ "${1:-"--pull"}" = "--nopull" ]; then
		#if I didn't manage to advance the branch, I don't really need to reload since there were no new files, but if I passed nopull I probably want to debug local changes -> do reload
		omz reload
	fi
}

#This function may be overwritten in Extensions to update those extensions along with ShellTools
#shellcheck disable=SC2120 # reason: The uzparameter is optional, therefore it needn't be used. it's only use is in an alias for quicker development
UpdateShellTools(){
	ST_CoreUpdate "$@"
}

#Notify about updates if the currently checked out branch is behind origin
ST_NUM_UPDATE_COMMITS="$(git -C "$ST_SRC" status -uno --ignore-submodules=all -b --porcelain=v2 | sed -nE 's~# branch.ab.*-([0-9]+)~\1~p')"
if [ "$ST_NUM_UPDATE_COMMITS" -gt 0 ]; then
#if [ "$(git -C "$ST_SRC" status -uno --ignore-submodules=all -b --porcelain=v2 | grep  branch.ab)" != "# branch.ab +0 -0" ]; then
	printf "There are ShellTools Updates available on the current branch (%s, %s commit(s)).\nDo you wish to update now [Y/n] " "$(git -C "$ST_SRC" symbolic-ref --short HEAD)" "$ST_NUM_UPDATE_COMMITS"
	read -r -k 1 versionconfirm
	printf '\n'
	case $versionconfirm in
		[Yy][Ee][Ss] | [Yy] )
			UpdateShellTools
			;;
		* )
			echo "not updating. You can manually update by running 'uz' or 'UpdateShellTools'"
			;;
	esac
fi
unset ST_NUM_UPDATE_COMMITS

#Ensure the config directory and at the basic elfs exist
if [ ! -e "$ST_CFG" ] || [ ! -e "$ST_CFG/repotools.elf" ]; then
	echo "$ST_CFG directory or repotools binary didn't exist -> creating now"
	mkdir -p "$ST_CFG"
	UpdateShellTools --nopull
fi

printf "\e[38;5;240mLoading Scripts from\e[0m %s\n" "$ST_SRC"
printf "last commit %s, %s local changes\n" "$(git -C "$ST_SRC" log --branches --remotes --tags --notes --pretty='[%C(brightblack)%as/%C(auto)%ar]%d' HEAD -1)" "$(git -C "$ST_SRC" status --ignore-submodules=dirty --porcelain=v2 -b --show-stash| grep -cv '# branch')"

PROXY_HOST=""
export PROXY_HOST
PROXY_USER=""
export PROXY_USER
PROXY_PORT=""
export PROXY_PORT
PROXY_NOAPT=1
export PROXY_NOAPT


export EDITOR=nano

#script invocations
alias pack='$ST_SRC/GENERAL/archive.sh -p'
alias unpack='$ST_SRC/GENERAL/archive.sh -u'
alias search='$ST_SRC/GENERAL/search.sh -p'
alias contentsearch='$ST_SRC/GENERAL/search.sh -c -p'
alias sysupdate='$ST_SRC/GENERAL/AptTools.sh --full-update'
alias aptcheck='$ST_SRC/GENERAL/AptTools.sh --check'
alias aptstatus='$ST_SRC/GENERAL/AptTools.sh --unattended'
alias acat='$ST_CFG/assemblecat.elf'
alias cat-assemble='$ST_CFG/assemblecat.elf'
alias argtest='$ST_CFG/argtest.elf'
alias rm-recurse='$ST_CFG/rm_recurse.elf'

alias force-git-enable='touch "$ST_CFG/forcegit"'
alias force-git-disable='rm "$ST_CFG/forcegit"'

#custom git log
#e -> extended (name), x-> exact (author/committer)
alias   gitlog="git log --branches --remotes --tags --graph --notes --pretty=\"%C(auto)%h [%C(brightblack)%as/%C(auto)%ar]%d %C(brightblack)%ae:%C(auto) %s\" HEAD"
alias  egitlog="git log --branches --remotes --tags --graph --notes --pretty=\"%C(auto)%h [%C(brightblack)%as/%C(auto)%ar]%d %C(brightblack)%ae(%an):%C(auto) %s\" HEAD"
alias  xgitlog="git log --branches --remotes --tags --graph --notes --pretty=\"%C(auto)%h [%C(brightblack)%as|%cs/%C(auto)%ar|%cr]%d %C(brightblack)%ae|%ce:%C(auto) %s\" HEAD"
alias exgitlog="git log --branches --remotes --tags --graph --notes --pretty=\"%C(auto)%h [%C(brightblack)%as|%cs/%C(auto)%ar|%cr]%d %C(brightblack)%ae(%an)|%ce(%cn):%C(auto) %s\" HEAD"
alias procowners="sudo ps axo user:64 |sort|uniq"
alias gitupdate='git submodule foreach --recursive '\''{ if git symbolic-ref --short HEAD >/dev/null 2>&1 ; then git pull --ff-only --prune ; else printf "%s is not on a branch -> skip\n" "$(pwd)" ; fi }'\'' ; git pull --ff-only --prune'
alias ff='STBREF="$(git symbolic-ref --short HEAD 2>/dev/null)" ; if [ -n "$STBREF" ]; then ;  git pull --ff-only ; else ; echo "cannot determine branch -> no action" ; fi ; unset STBREF'
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
alias zshrc='nano "$HOME/.zshrc"'
alias cfg='nano "$ST_CFG/config.cfg"'
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
alias uz=UpdateShellTools
alias iuz="UpdateShellTools --nopull"
#UpdateZSH was the old name for my update function, it now is included as a legacy option
alias UpdateZSH=UpdateShellTools
alias ss=SystemStatus
alias syss=SystemStatus
alias pkgstatus=SystemStatus
alias pkgstat=SystemStatus
alias or="omz reload"
alias hist='cat "$HOME/.zsh_history" "$HOME/.bash_history" | less'
alias qlac="qalc"
alias qcla="qalc"
alias calc="qalc"
alias qacl="qalc"

searchapt(){
	printf "Apt packages:\n"
	apt-cache search "$1" | grep "$1"
	printf "\nInstallation status:\n"
	dpkg -l |grep "$1"
}

cleanFile(){
	#the reason I use a temp file is to avoid the same file on both ends of the pipe.
	___tempfile_local=$(mktemp)
	LC_ALL=C LANG=C LC_COLLATE=C sort -Vu < "$1" > "$___tempfile_local"
	mv "$___tempfile_local" "$1"
}

sortFile(){
	___tempfile_local=$(mktemp)
	LC_ALL=C LANG=C LC_COLLATE=C sort -V < "$1" > "$___tempfile_local"
	mv "$___tempfile_local" "$1"
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
	if [ "$PROXY_NOAPT" -eq 0 ]; then
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
	fi
}

HasLocalProxyServer(){
	#return default false if it's not overwriten in other scripts
	#NOTE: it WILL be overwriten in custom non-public extensions
	echo "default false for Local Proxy"
	return 1
}

TestProxyExtension(){
	#default return false if it's not overwriten in other scripts
	#NOTE it WILL be overwriten in custom non-public extensions
	return 1
}

enableProxy(){
	#if any of the PROXY* variables I would be setting are already there, abort this. (unless configured)
	if [ -n "$PROXY_HOST" ] ; then
		SetUpAptProxyConfigFile

		if HasLocalProxyServer ; then
			#echo "woking with local proxy on $PROXY_HOST, port $PROXY_PORT"
			#PROXY_NAME will be set in a nonpublic extension
			echo "detected $PROXY_NAME on port $PROXY_PORT"
			PROXY_STRING="http://$PROXY_HOST:$PROXY_PORT"
			export {http,https,ftp}_proxy="$PROXY_STRING"
			export {HTTP,HTTPS,FTP}_PROXY="$PROXY_STRING"
			if [ "$PROXY_NOAPT" -eq 0 ]; then
				printf 'Acquire {\n  HTTP::proxy "%s";\n  HTTPS::proxy "%s";\n}\n' "$PROXY_STRING" "$PROXY_STRING" > /etc/apt/apt.conf.d/proxy
			fi
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
			if [ "$PROXY_NOAPT" -eq 0 ]; then
				printf 'Acquire {\n  HTTP::proxy "%s";\n  HTTPS::proxy "%s";\n}\n' "$PROXY_STRING" "$PROXY_STRING" > /etc/apt/apt.conf.d/proxy
			fi
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
			printf "curl cannot resolve proxy host (%s) for %s, tentatively leaving proxy enabled, but might not work" "$PROXY_HOST" "$PROXY_NAME"
		elif [ 6 -eq $CurlRes ]; then
			printf "curl cannot resolve test host (%s) for %s, tentatively leaving proxy enabled, but might not work" "$ConnTestURL" "$PROXY_NAME"
		else
			#no connection
			printf "Connection to %s via %s not possible (curl exit code %i). Check your Password?\n" "$ConnTestURL" "$PROXY_NAME" $CurlRes
			TestProxyExtension $CurlRes $ConnTestURL
			local LocalProxyTestResult=$?
			if [ ! 0 -eq $LocalProxyTestResult ]; then
				disableProxy
			fi
		fi
	else
		echo "PROXY_HOST not provided -> NO ACTION"
	fi
}

wat(){
	apropos "$1"
	type "$1"
	printf "from Package:\n"
	dpkg -S "$(which "$1")"
	printf "\nalso located at:\n"
	builtin where "$1"
	printf "\ncontained in following packages\n"
	dpkg -S "$1"
}

disableProxy(){
	unset {http,https,ftp,no}_proxy
	if [ "$PROXY_NOAPT" -eq 0 ]; then
		echo "">/etc/apt/apt.conf.d/proxy
	fi
	echo "disabled proxy"
}

sysver(){
	#the following line parses /etc/os-release for the pretty_name and the version and kind of concatenates them. The pretty name usually contains a shorter version, so assemblecat is used which omits the common parts
	#generally pretty_name will be the topmost entry and version will be somewhere down the line, with version_id below that. Alpine doesn't have version, just version_id, therefore I had to also allow version_id, but due to the always-greedy setup of sed
	#I would always capture version_id which doesn't contain the version codename. Furthermore in alpine pretty_name is BELOW version_id, so I need to sort the input to be able to regex it. then, to avoid always matching version_id, I reverse sort it,
	#this makes the first greedy match contain version_id as long as there is version, version_id therefore only is a fallback (I had to flip the capture groups in the regex as well)
	#--equal=\" v\" tells assemblecat to treat ' ' and 'v' the same to allow assembling of "Alpine Linux v3.17" with " 3.17.0" into "Alpine Linux v3.17.0". I wouldn't need the equality list if there wasn't a space in the second string
	#however most distros need that space to avoid "Kali GNU/Linux Rolling2023.3"


	eval "$ST_CFG/assemblecat.elf --ignore-case --equal=\" v\" $(echo "$XDG_CONFIG_DIRS" | grep -io ".ubuntu" | sed -E 's~^[^a-zA-Z]?(.)~\U\1~') $(sort -r < /etc/os-release | tr -d '\n' | sed -nE 's~.*VERSION(_ID)?="?([-a-zA-Z0-9\._ \(\)/]+)"?.*PRETTY_NAME="([-a-zA-Z0-9\._ \(\)/]+)".*~"\3" " \2"~p')" |tr -d '\n'
	if [ -n "$XDG_CURRENT_DESKTOP" ]; then
		printf " using %s" "$XDG_CURRENT_DESKTOP"
	fi
	#XDG_SESSION_TYPE can be set, even if there isn't a graphical interface (eg, on TTY)
	if [ -n "$XDG_SESSION_TYPE" ]; then
		printf " (%s)" "$XDG_SESSION_TYPE"
	fi
	printf " [%s %s %s]\n" "$(uname --operating-system)" "$(uname --kernel-release)" "$(uname --machine)" #machine is x86-64 or arm or something
}

shellver(){
	printf "\e[38;5;240mRunning Scripts from \e[0m%s\n" "$ST_SRC"
	if [ -e "${ST_SRC}/../ShellToolsExtensionLoader.sh" ]; then
		printf "\e[38;5;240mwith Extensions from \e[0m%s\n" "$(realpath "$(dirname "${ST_SRC}/../ShellToolsExtensionLoader.sh")")"
	fi
}

SystemStatus(){
	"$ST_SRC/GENERAL/FWTools.sh"
	"$ST_SRC/GENERAL/AptTools.sh" --unattended
	"$ST_SRC/GENERAL/FlatpakTools.sh"
	"$ST_SRC/GENERAL/SnapTools.sh"
	sh -c '"$ST_SRC/ZSH/UpdateChecker.sh" &'
}

if [ -e "$ST_SRC/../CUSTOM.sh" ]; then
	echo "Loading Custom Extensions"
	# shellcheck disable=SC1091 # reason: this file may very well not exist, as it will only exist for some systems where there are non-public ShellTools Extensions.
	# shellcheck disable=SC1090 # shellcheck can't find the file, obviously
	. "$ST_SRC/../CUSTOM.sh"
fi

IsWSL=0
#use cmd.exe to get windows username, discard cmd's stderr (usually because cmd prints a warning if this was called from somewhere within the linux filesystem), then strip CRLF from the result
WinUser="$(cmd.exe /c "echo %USERNAME%" 2>/dev/null | tr -d '\n\r')"
#the reason I don't just check $WSL_DISTRO_NAME is because I want to know if it's WSL1 or WSL2. there may be differences I need to address at some point
#this is a command that's EXPECTED to fail every time this is run on a non-WSL installation, so the stderr redirect and masking of the return code are intentional
#iconv is a tool to convert character encodings. this is needed to change windows's UTF16LE to the UTF-8 I want
{ WSL_VERSION="$(/mnt/c/Windows/System32/wsl.exe -l -v | iconv -f unicode|grep -E "\b$WSL_DISTRO_NAME\s+Running" | awk '{print $NF}' | cut -c-1)"; } 2> /dev/null || true
if [ -n "$WSL_VERSION" ]; then
	IsWSL=1
	export WSL_VERSION
fi

export IsWSL

if [ $IsWSL -eq 1 ]; then
	export WinUser
	#shellcheck source=WSL.sh
	. "$ST_SRC/ZSH/WSL.sh"
else
	unset WinUser
	#check if this is debian-esque, if it's not, it's not supported, exit immediatley
	#shellcheck source=NATIVE.sh
	. "$ST_SRC/ZSH/NATIVE.sh"
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

# shellcheck disable=SC2046 #reason: I DO want wordsplitting to happen on the date
printf "System up since %s (%s)\n" "$(uptime --since)" "$("$ST_CFG/st_timer.elf" STOPHUMAN $(uptime --since |tr ':-' ' '))"

if [ "${ST_DO_SYSTEM_UPDATE_CHECK_ON_STARTUP:-1}" -ne 0 ];then
	SystemStatus
else
	if [ -f /var/run/reboot-required ]; then
		cat /var/run/reboot-required
	fi
	if [ -f /var/run/reboot-required.pkgs ]; then
		echo "Reboot is required to apply updates for following packages:"
		sort < /var/run/reboot-required.pkgs | uniq
	fi
fi

#this is just to force the initial return code to be 0/OK when opening a new shell
true
