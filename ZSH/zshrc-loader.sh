#!/bin/false
# shellcheck shell=sh

## this script must be sourced in zshrc as follows (obviously adjust CODE_LOC):
##
## CODE_LOC="/mnt/e/ShellTools"
## export CODE_LOC
## . "$CODE_LOC/ZSH/zshrc-loader.sh"
##
##NOTE: in bash/ksh/zsh export CODE_LOC="/mnt/e/CODE would also be valid but not in POSIX where the assignement and export must be seperate"

#the . somescript is equivalent to source somescript but is portable since it's POSIX compliant
# shellcheck source=./general.sh
. "$CODE_LOC/ZSH/general.sh"

#the reason I don't just check $WSL_DISTRO_NAME is because I want to know if it's WSL1 or WSL2. there may be differences I need to address at some point
#this is a command that's EXPECTED to fail every time this is run on a non-WSL installation, so the stderr redirect and masking of the return code are intentional
#iconv is a tool to convert character encodings. this is needed to change windows's UTF16LE to the UTF-8 I want
{ WSL_VERSION="$(/mnt/c/Windows/System32/wsl.exe -l -v | iconv -f unicode|grep -E "\b$WSL_DISTRO_NAME\s+Running" | awk '{print $NF}' | cut -c-1)"; } 2> /dev/null || true
if [ -z "$WSL_VERSION" ]; then
	echo "native -> don't source WSL"
else
	export WSL_VERSION
	# shellcheck source=./wsl_common.sh
	. "$CODE_LOC/BAT_VBS/ZSH/wsl_common.sh"
fi

echo "done loading utility scripts"
printf "Welcome to "
#the following line parses /etc/os-release for the pretty_name and the version and kind of concatenates them. The pretty name usually contains a shorter version, so assemblecat is used which omits the common parts
#generally pretty_name will be the topmost entry and version will be somewhere down the line, with version_id below that. Alpine doesn't have version, just version_id, therefore I had to also allow version_id, but due to the always-greedy setup of sed
#I would always capture version_id which doesn't contain the version codename. Furthermore in alpine pretty_name is BELOW version_id, so I need to sort the input to be able to regex it. then, to avoid always matching version_id, I reverse sort it,
#this makes the first greedy match contain version_id as long as there is version, version_id therefore only is a fallback (I had to flip the capture groups in the regex as well)
#--equal=\" v\" tells assemblecat to treat ' ' and 'v' the same to allow assembling of "Alpine Linux v3.17" with " 3.17.0" into "Alpine Linux v3.17.0". I wouldn't need the equality list if there wasn#t a space in the second string
#however most distros need that space to avoid "Kali GNU/Linux Rolling2023.3"
eval "$HOME/assemblecat.elf --equal=\" v\" $(sort -r < /etc/os-release | tr -d '\n' | sed -nE 's~.*VERSION(_ID)?="?([-a-zA-Z0-9\._ \(\)/]+)"?.*PRETTY_NAME="([-a-zA-Z0-9\._ \(\)/]+)".*~"\3" " \2"~p')" |tr -d '\n'
printf " [%s %s %s]\n" "$(uname --operating-system)" "$(uname --kernel-release)" "$(uname --machine)" #machine is x86-64 or arm sor something
# if [ "$(uname --machine)" != "x86_64" ]; then
# 	echo "I had a comment about the hardware platform here, but I'll keep that to myself"
# fi
if [ "$WSL_VERSION" -ne 1 ]; then
	#system load is hardcoded in WSL as per https://github.com/microsoft/WSL/issues/945 and allegedly fixed in wsl2 as per https://github.com/microsoft/WSL/issues/2814
	printf "System load averages (1/5/15 min) are %s out of %s\n" "$(cut -f1-3 -d ' ' /proc/loadavg | tr ' ' '/')" "$(grep -c ^processor /proc/cpuinfo).0"
fi

"$CODE_LOC/BAT_VBS/AptTools.sh" --unattended
