#!/bin/false
#this should only be sourced or '.' included, not called.

#zsh prompt expansion web doc is at https://zsh.sourceforge.io/Doc/Release/index.html#Top
#color palette can be found by executing command spectrum_ls
#abbreviated info from https://blog.carbonfive.com/writing-zsh-themes-a-quickref/
#PROMPT='whatever is in here is on the left of the text input prompt'
#RPROMPT='whatever is in here is in the right side of the screen of the prompt'
#anyhing to be passed into PROMPT will be interpreted by the shell and should NOT be double quoted.
#colours are set INLINE using %{$fg[...]%} where ... is a web-safe colour of black/blue/red/magenta/green/cyan/yellow/white
#to use a different colour than the eight defined websafe ones use FG instead of fg and insert the colour code obtained from the spectrum_ls command
#the same applies to Background colours, just use BG/bg instead of FG/fg
#there is a shorthand for both: %F{...} for text(foreground) colour and %K{...} for background (same for text and number, no more case difference)
#to reset a colour back to default use %{$reset_color%} (restets both text and background) or the shorthand of %f for text and %k for background
#\u2718 is errorX (like for unclean git)
#prompt variables: %n -> current user   |   %0 -> pwd   %1 -> current dir   %2 -> current dir + parent dir etc. (if used wth a ~ eg %0~ it'd abbreviate paths with ~)
#%T gives time as hh:mm   %* gives time as hh:mm:ss   %D{...} is custom strftime format
#%? returns status code of last command
#conditional syntax: %(TestChar.TrueVal.FalseVal) (for testChar doc see https://zsh.sourceforge.io/Doc/Release/Prompt-Expansion.html#Conditional-Substrings-in-Prompts)
#conditional: the seperator doesn't need to be . it is arbitrary


#### Time Taken for Command Helper functions
#just declare vars in a script local scope
local CmdStartTime=""
local CmdDur="0s"

GetTimeStart (){
	#this function is executed via zsh hook on preexec (ie just before the command is actually executed)
	#%s is seconds since UNIX TIME; %3N gives the first three digits of the current Nanoseconds
	CmdStartTime="$(~/timer-zsh.elf START)"
}

GetProxyInfo(){
	local PROXY_STATE_RES
	if [ -r /etc/apt/apt.conf.d/proxy ]; then
		local apt_proxy_state_http
		local apt_proxy_state_https
		local valthing
        valthing=$(tr -d '\n' < /etc/apt/apt.conf.d/proxy)
		apt_proxy_state_http=$(echo "$valthing"  | grep -G 'Acquire.*{.*HTTP::proxy.*;}')
		apt_proxy_state_https=$(echo "$valthing" | grep -G 'Acquire.*{.*HTTPS::proxy.*;}')
		if [ "$apt_proxy_state_http" ]; then
			if [ "$apt_proxy_state_https" ]; then
				PROXY_STATE_RES="$PROXY_STATE_RES%F{green}A%f" #both
			else
				PROXY_STATE_RES="$PROXY_STATE_RES%F{red}A%f" #http but not https
			fi
		else
			if [ "$apt_proxy_state_https" ]; then
				PROXY_STATE_RES="$PROXY_STATE_RES%F{orange}A%f" #https but not http
			fi
		fi
	fi
	if [ "$http_proxy" ]; then
		if [ "$https_proxy" ]; then
			PROXY_STATE_RES="$PROXY_STATE_RES%F{green}H%f" #both
		else
			PROXY_STATE_RES="$PROXY_STATE_RES%F{red}H%f" #http but not https
		fi
	else
		if [ "$https_proxy" ]; then
			PROXY_STATE_RES="$PROXY_STATE_RES%F{orange}H%f" #https but not http
		fi
	fi
	if [ "$no_proxy" ]; then
		PROXY_STATE_RES="$PROXY_STATE_RES%F{green}N%f"
	fi
	if [ "$ftp_proxy" ]; then
		PROXY_STATE_RES="$PROXY_STATE_RES%F{green}F%f"
	fi

	if [ "$PROXY_STATE_RES" ]; then
		echo "[$PROXY_STATE_RES]:"
	else
		echo ""
	fi
}

CalcTimeDiff (){
	#this function takes the previously stored start time and calculates the time difference to now
	#this function is automatically called via zsh hook on precmd (ie immediatly after the last command execution finishes
	#but before the next prompt is prepared)
	if [ "$CmdStartTime" ]; then
		CmdDur="$(~/timer-zsh.elf STOP $CmdStartTime)"
		CmdStartTime=""
	fi
	#passion theme had another if [ "${#CmdDur}" = "2" ]; then CmdDur="0$CmdDur" fi to make sure to have 0.X and not just .X for sub 1 second commands
	#echo "$CmdDur"
}
setopt prompt_subst # das wird von oh-my-zsh ohnehin gesetzt, es schadet aber nicht
#die funktion hiervon ist dass prompt substitution überhaupt geht und der promprt nicht ein vorher definierter statischer string ist
autoload -Uz add-zsh-hook # load add-zsh-hook with zsh (-z) and suppress aliases (-U)
#https://stackoverflow.com/questions/30840651/what-does-autoload-do-in-zsh
add-zsh-hook preexec GetTimeStart
add-zsh-hook precmd CalcTimeDiff
#### End Time Taken for Command Helper Functions



function GetBackgroundTaskInfo (){
	#this calls `jobs` and uses `sed` to un following regex: ^\[(\d+)\]\s+([+|-]?)\s*(\w).+$
	#(basically takes the number in square brackets, an optional + or - and the first letter of the status as capture groups and prints them in the order 1 3 2)
	#then tr is used to get rid of linebreaks and shove everything to uppercase. finally rev|cut -c2-|rev chops trailing whitespace off
	local background_task_info
	background_task_info="$(jobs|sed 's/^\[\([0-9]\+\)\][^a-zA-Z+\-]*\([+\-]\?\)[^a-zA-Z+\-]*\(.\).*$/\1\3\2/'|tr 'a-z\n' 'A-Z '|rev|cut -c2- |rev)"
	if [ ${#background_task_info} -eq "0" ]; then
		echo ""
	else
		echo " [$background_task_info]"
	fi
}

function GetGitRemoteInformation (){
	#git://github.com/something/REPO                -> REPO from git:github.com
	#http://www.github.com/something/REPO           -> REPO from http:github.com
	#ssh://git@someurl.de:1234/path/REPO     -> REPO from ssh:git@someurl.de:1234
	#ssh://git@some-url.de:1234/path/REPO    -> REPO from ssh:git@some-url.de:1234
	#ssh://git@some_url.de:1234/path/REPO    -> REPO from ssh:git@some_url.de:1234
	#git@127.0.0.1:/path/REPO                   -> REPO from ssh:git@127.0.0.1
	#git@27.0.0.1/path/REPO                    -> REPO from ssh:git@127.0.0.1
	#/data/repos/REPO                               -> REPO from /data/repos
	#../../REPO                                     -> REOP from ../../

	#if it contains @ but not //, prepend "ssh://"
	# a remote url can be done like:    (?<proto>\w+?)://(?:www.)?(?<username>\w+@)?(?<host>[0-9a-zA-Z\.\-_]+):?(?<port>:\d*)?(?:/\S+)*/(?<repo>\S+)       replace with        ${repo} from ${proto}:${username}${host}${port}
	#but note: don't use / as a seperator, ~ is a better idea.
	#if not, it's local

	#I want to have the info: 1.Protocol (ssh/local/git)  2a.(if local, path [abs or rel])  2b.(if remote, host/IP+port)
	#at the moment port isn't implemented
	#if it's not a repo, do nothing
	#local repoinfo="$(git remote -v|head -1|sed 's~^.*\([ssh|http.]\?\).*@\([a-zA-Z0-9\.\-\_]*\).*$~\1:\2~')"
	##local fulltextremote
	##fulltextremote="$(~/repotools.elf "$(git ls-remote --get-url origin)" "$(pwd)")"
	if [ "$fulltextremote" != 'origin' ]; then
	##	local localrepos
	##	# this used to work, but an update to realpath broke it, since realpath started to spit out something that's neither an URI nor a direct path
	##	#localrepos="$(realpath -q -s --relative-to="$(pwd)" "$fulltextremote")" 
	##	#the grep -v is to remove the contents of fulltextremote if it starts with something://, the final "" is there as a kind of default value for realpath since realpath complains about missing argument if grep fully removes the fulltextremote
	##	# a remote of git@127.0.0.1:/data/repos/somerepo (as seen in fulltextremote) breaks
	##	#if I have something of the type of user@host or user@url -> prepend ssh://
	##	localrepos="$(realpath -q -s --relative-to="$(pwd)" "$( echo "$fulltextremote" | grep -v ".\+://")" "")"
	##	local remoteRepos
	##	remoteRepos="$(sed -n 's|^\([a-zA-Z0-9]\+\)://\([a-zA-Z0-9]\+@\)\?\([a-zA-Z0-9._\-]\+\).*$|\1:\3|p' <<< "$fulltextremote")"
	##	local repoName
	##	repoName="$(echo "$fulltextremote" | rev | cut -f 1 -d '/' | rev)"
	##	echo "%F{009}$repoName %F{001}from %F{009}$localrepos$remoteRepos %f"
	echo "$(~/repotools.elf "$(git ls-remote --get-url origin)")"
	else
		echo ""
	fi
}

#adapted from various answers from https://stackoverflow.com/questions/44544385/how-to-find-primary-ip-address-on-my-linux-machine (the double grep)
#and https://unix.stackexchange.com/questions/14961/how-to-find-out-which-interface-am-i-using-for-connecting-to-the-internet (the bit about ip route ls |grep defult (the sed is my own work))
#another way would be to cause a dns lookup like so and grep that ip route get 8.8.8.8
function GetLocalIP (){
	IpAddrList=""
	for i in $(ip route ls | grep default | sed 's|^.*dev \([a-zA-Z0-9]\+\).*$|\1|') # get the devices for any 'default routes'
	do
		#lookup the IP for those devices
		#the full command for 'ip a s dev <device>' is 'ip addr show dev <device>'
		isupstate="$(ip a s dev "$i" | tr '\n' ' ' | grep -Eo '.*<.*UP.*>.*inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*')"
		if [ "$isupstate" ]; then
			IpAddrList="$IpAddrList $i:$isupstate"
		fi
	done

	if [ ! "$IpAddrList" ]; then
		IpAddrList=" NC"
	fi

	echo "$IpAddrList"|cut -c2- #bin the first space
}

#/sys/class/power_supply/battery/status Discharging/Full/Charging -> 
#/sys/class/power_supply/battery/capacity Percentage
#↯≻▽▲
#possibly, depending on the system it'd be BAT0 or BAT1 instead of battery
function PowerState (){
	local BatSource
	if [ -r /sys/class/power_supply/battery/capacity ] && [ -r /sys/class/power_supply/battery/status ]; then
		BatSource='battery'
	elif [ -r /sys/class/power_supply/BAT0/capacity ] && [ -r /sys/class/power_supply/BAT0/status ]; then
		BatSource='BAT0'
	elif [ -r /sys/class/power_supply/BAT1/capacity ] && [ -r /sys/class/power_supply/BAT1/status ]; then
		BatSource='BAT1'
	else
		#either of the two needed files isn't readable -> just skip this
		echo ""
		return
	fi
	local __PWR_COND_STATUS
	local __PWR_COND_LVL
	__PWR_COND_LVL=$(cat /sys/class/power_supply/$BatSource/capacity)
	case $(cat /sys/class/power_supply/$BatSource/status) in
	Charging*)
		__PWR_COND_STATUS="▲"
		;;
	Discharging*)
		__PWR_COND_STATUS="▽"
		;;
	Full*)
		__PWR_COND_STATUS="≻"
		;;
	'Not charging'*)
		__PWR_COND_STATUS="%F{red}◊"
		;;
	*)
		#realistically this shouldn't happen, but if there's a state I missed I'll find out what is was
		__PWR_COND_STATUS="$(cat /sys/class/power_supply/$BatSource/status)"
		;;
	esac
	#also possibly interessant char:↯
	echo "%B $__PWR_COND_STATUS $__PWR_COND_LVL%%%f%b"
}

local user_at_host="%F{010}%n%F{007}@%F{033}%m:/dev/%y%f" # 'user@host:/dev/terminaldevice'
local WorkingDirectory='%F{cyan}%B%c%b%f' # 'directory'

#$(git_prompt_info) is built as follows: PREFIX+branch+[DIRTY|CLEAN]+SUFFIX  //Dirty/Clean je nachdem was zutrifft (there's also extra stuff in OhMyZsh)
ZSH_THEME_GIT_PROMPT_PREFIX="%F{001}git:(%F{009}"
ZSH_THEME_GIT_PROMPT_SUFFIX="%f "
ZSH_THEME_GIT_PROMPT_DIRTY="%F{001}) %F{011}\u2573"
ZSH_THEME_GIT_PROMPT_CLEAN="%F{001})"
PROMPT='${user_at_host} $(git_prompt_info)$(GetGitRemoteInformation)$WorkingDirectory %B%(?:%F{green}:%F{red})[$CmdDur:%?]➜%f%b '
RPROMPT='%D{%T UTC%z (%Z)}$(GetBackgroundTaskInfo) $(GetProxyInfo)$(GetLocalIP)$(PowerState)'
echo "SetupDone"
