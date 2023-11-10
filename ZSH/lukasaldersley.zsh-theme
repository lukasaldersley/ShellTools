#!/bin/false
# shellcheck shell=bash
#this should only be sourced or '.' included, not called.
echo "Sourcing $0"

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
#prompt variables: %n -> current user   |   %0 -> pwd   %1 -> current dir   %2 -> current dir + parent dir etc. (if used wth a ~ eg %0~ it'd abbreviate paths with ~)
#%T gives time as hh:mm   %* gives time as hh:mm:ss   %D{...} is custom strftime format
#%? returns status code of last command
#conditional syntax: %(TestChar.TrueVal.FalseVal) (for testChar doc see https://zsh.sourceforge.io/Doc/Release/Prompt-Expansion.html#Conditional-Substrings-in-Prompts)
#conditional: the seperator doesn't need to be . it is arbitrary
#https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit


#this will be set by oh-my-zsh anyway but it won't hurt to, for sanity, set it here as well
#this is required (at any point) to enable any form if prompt susbstitution/to enable anything beyond a static string without vars as prompt
setopt prompt_subst
setopt PROMPT_SUBST
#autoload here means I promise to this system a function add-zsh-hook will exist at runtime but it isn't known yet where it will be.
#even if it's unknown that function can then be used here without issues
#https://stackoverflow.com/questions/30840651/what-does-autoload-do-in-zsh
autoload -Uz add-zsh-hook # load add-zsh-hook with zsh (-z) and suppress aliases (-U)

#### Time Taken for Command Helper functions
#just declare vars in a script local scope
local CmdStartTime=""
local CmdDur="0s"

GetTimeStart (){
	#this function is executed via zsh hook on preexec (ie just before the command is actually executed)
	#%s is seconds since UNIX TIME; %3N gives the first three digits of the current Nanoseconds
	CmdStartTime="$(~/.shelltools/timer-zsh.elf START)"
}

CalcTimeDiff (){
	#this function takes the previously stored start time and calculates the time difference to now
	#this function is automatically called via zsh hook on precmd (ie immediatly after the last command execution finishes
	#but before the next prompt is prepared)
	#the 'passion' theme shipped with oh-my-zsh does this entirely in shell, but I decided that's too slow and imprecise -> c binary compiled from ./timer.c
	if [ "$CmdStartTime" ]; then
		CmdDur="$(~/.shelltools/timer-zsh.elf STOP "$CmdStartTime")"
		CmdStartTime=""
	fi
}
add-zsh-hook preexec GetTimeStart
add-zsh-hook precmd CalcTimeDiff
#### End Time Taken for Command Helper Functions

function GetSSHInfo(){
	#this attempts to parse the SSH_CONNECTION env-variable to get '<SSH: clientIP:clientPort -> serverIP:serverPort>'
	echo "$SSH_CONNECTION" | sed -nE 's~^([-0-9a-zA-Z_\.]+) ([0-9]+) ([-0-9a-zA-Z_\.]+) ([0-9]+)$~<SSH: \1:\2 -> \3:\4> ~p'
}

function GetBackgroundTaskInfo (){
	#this calls `jobs` and uses `sed` to un following regex: ^\[(\d+)\]\s+([+|-]?)\s*(\w).+$
	#(basically takes the number in square brackets, an optional + or - and the first letter of the status as capture groups and prints them in the order 1 3 2)
	#then tr is used to get rid of linebreaks and shove everything to uppercase. finally rev|cut -c2-|rev chops trailing whitespace off
	#NOTE: this isn't straight printed, repotools takes this as input and computes the number of jobs and may then print it if there's space
	local background_task_info
	background_task_info="$(jobs|sed -nE 's~^\[([0-9]+)\][^-a-zA-Z+]*([-+]?)[^-a-zA-Z+]*(.).*$~\1\3\2~p'|tr 'a-z\n' 'A-Z '|rev|cut -c2- |rev)"
	if [ ! -n "${background_task_info}" ]; then
		echo ""
	else
		echo ": {$background_task_info}"
	fi
}

#adapted from various answers from https://stackoverflow.com/questions/44544385/how-to-find-primary-ip-address-on-my-linux-machine (the double grep)
#and https://unix.stackexchange.com/questions/14961/how-to-find-out-which-interface-am-i-using-for-connecting-to-the-internet (the bit about ip route ls |grep defult (the sed is my own work))
#another way would be to cause a dns lookup like so and grep that ip route get 8.8.8.8
function GetLocalIP (){
	IpAddrList=""
	for i in $(ip route ls | grep default | sed 's|^.*dev \([a-zA-Z0-9]\+\).*$|\1|') # get the devices for any 'default routes'
	do
		#TODO possibly display metric (lower is better) or mark lowest metric route with a star
		#TODO find out what metrics I get if a system has two valid conns (eg serenity) -> both connections (provided they are on the same network have the same metric (wsl:0; win:35), this holds true even in windows (cmd: route print))
		#I tested conecting to mobile hostpot while also on enternet->result: in wsl both have metric 0, whereas in windows the wifi from mobile tethering had metric 50 while ethernet remained at 35
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

	echo "$IpAddrList" #|cut -c2- #bin the first space
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
				PROXY_STATE_RES="$PROXY_STATE_RES%F{002}A%f" #both %green
			else
				PROXY_STATE_RES="$PROXY_STATE_RES%F{001}A%f" #http but not https %red
			fi
		else
			if [ "$apt_proxy_state_https" ]; then
				PROXY_STATE_RES="$PROXY_STATE_RES%F{202}A%f" #https but not http %orange
			fi
		fi
	fi
	if [ "$http_proxy" ]; then
		if [ "$https_proxy" ]; then
			PROXY_STATE_RES="$PROXY_STATE_RES%F{002}H%f" #both
		else
			PROXY_STATE_RES="$PROXY_STATE_RES%F{001}H%f" #http but not https
		fi
	else
		if [ "$https_proxy" ]; then
			PROXY_STATE_RES="$PROXY_STATE_RES%F{202}H%f" #https but not http
		fi
	fi
	if [ "$no_proxy" ]; then
		PROXY_STATE_RES="$PROXY_STATE_RES%F{002}N%f"
	fi
	if [ "$ftp_proxy" ]; then
		PROXY_STATE_RES="$PROXY_STATE_RES%F{002}F%f"
	fi

	if [ "$PROXY_STATE_RES" ]; then
		echo " [$PROXY_STATE_RES]"
	else
		echo ""
	fi
}

function PowerState (){
	local BatSource
	#dependig on the system the battery might be named differently, this isto accomodate that
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
	local __PWR_EXT_STATUS

	#this is to determine the AC status. at least on WSL /sys/class/power_supply/ac is unused and mapped into usb instead.
	#on a Xubuntu VM there was /sys/class/power_supply/AC and that System also had Power status 'Unknown' (this VM was running in VirtualBox on top of an older Win10)
	local USB_PWR
	local AC_PWR
	if [ -r /sys/class/power_supply/usb/online ] && [ "$(cat /sys/class/power_supply/usb/online)" = "1" ]; then
		USB_PWR=1
	fi
	if [ -r /sys/class/power_supply/ac/online ] && [ "$(cat /sys/class/power_supply/ac/online)" = "1" ]; then
		AC_PWR=1
	elif [ -r /sys/class/power_supply/AC/online ] && [ "$(cat /sys/class/power_supply/AC/online)" = "1" ]; then
		AC_PWR=1
	fi
	__PWR_EXT_STATUS=""

	if [ "$AC_PWR" = 1 ] || [ "$USB_PWR" = 1 ]; then
		if [ -n "$WSL_VERSION" ];then
			#WSL->power always reported as USB -> no specific symbol
			__PWR_EXT_STATUS=" "
		else
			if [ "$AC_PWR" = 1 ] && [ ! "$USB_PWR" = 1 ]; then
				#AC
				__PWR_EXT_STATUS=" ≈"
			elif [ ! "$AC_PWR" = 1 ] && [ "$USB_PWR" = 1 ]; then
				#USB
				__PWR_EXT_STATUS=" ≛"
			else
				#BOTH
				__PWR_EXT_STATUS=" ⩰"
			fi
		fi
		__PWR_EXT_STATUS="$__PWR_EXT_STATUS↯"
	fi


	#this reads the battery status and appends % but because this is used in the prompt % needs to be escaped by another % infront
	__PWR_COND_LVL="$(cat /sys/class/power_supply/$BatSource/capacity)"

	case $(cat /sys/class/power_supply/$BatSource/status) in
	Charging*)
		__PWR_COND_STATUS="%F{157} ▲ "
		;;
	Discharging*)
		__PWR_COND_STATUS="%F{217} ▽ "
		;;
	Full*)
		__PWR_COND_STATUS="%F{157} ≻ "
		#if the status is "battery Full" yet the battery charge percentage is 0%, it's probably because the system is a desktop PC running wsl where there is no battery but wsl still reports as if there was one -> just blank the text
		if [ "${__PWR_COND_LVL}" = "0" ]; then
			echo ""
			return
		fi
		;;
	'Not charging'*)
		if [ "$__PWR_COND_LVL" -ge 95 ]; then
			__PWR_COND_STATUS=" %F{172}◊ "
		else
			__PWR_COND_STATUS=" %F{009}◊ "
		fi
		;;
	'Unknown'*)
		__PWR_COND_STATUS=" %F{009}!⌧? "
		;;
	*)
		#realistically this shouldn't happen, but if there's a state I missed I'll find out what is was
		__PWR_COND_STATUS="$(cat /sys/class/power_supply/$BatSource/status)"
		;;
	esac
	echo -e "%B$__PWR_COND_STATUS$__PWR_COND_LVL\a%%%f%b$__PWR_EXT_STATUS"
}

function MainPrompt(){
	print -P "\n$(~/.shelltools/repotools.elf --prompt -c"$COLUMNS" -d"$(print -P ":/dev/%y\a")" -i"$(print -P "$(GetLocalIP)\a")" -p"$(print -P "$(GetProxyInfo)\a")" -e"$(print -P "$(PowerState)\a")" -j"$(print -P "$(GetBackgroundTaskInfo)\a")" -l" [$SHLVL]" -s"$(print "$(GetSSHInfo)\a")" "$(pwd)")"
}

add-zsh-hook precmd MainPrompt

#PROMPT='└%F{cyan}%B %2~ %b%f%B%(?:%F{green}:%F{red})[$CmdDur:%?]➜%f%b  '
PROMPT='$("$HOME/.shelltools/repotools.elf" --lowprompt -r"$?" -c"$COLUMNS" -t"$CmdDur" "$(print -P '%~')")'

#To do multiline Prompts I am printing the top line before the prompt is ever computed.
#print -P is a zsh builtin (man zshmisc) that does the same substitution the prompt would.
#I had had the top line in PROMPT with $'\n' to create the linebreak, but there were some implications:
# + : The width of the top line would be recomputed every time the terminal size changed
# - : the prompt kept creeping up one line overwriting previous output whenever the window size changed
# - : the command buffer kept interfering with my two line prompt (especially when recalling very long input commands via 'arrow-up')
# - : GetBackgroundTasks was being executed, but kept returning empty strings (probably because jobs is a zsh builtin and the prompt building was happening in another subshell with no jobs or something)
# - : having a linebreak in PROMPT messed with the positioning of RPROMPT and the carret behaviour for editing typed commands
# - : hitting 'HOME' or 'POS1' often would send the carret to the top line and was showing editing that but in reality it was invisibly editing the command string
#Conclusion: Make top line single-fire evaluation

echo "done loading $(basename $0)"
