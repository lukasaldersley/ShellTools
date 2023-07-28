#!/bin/false
# shellcheck shell=sh

## this script must be sourced in zshrc as follows (obviously adjust CODE_LOC):
##
## CODE_LOC="/mnt/e/ShellTools"
## export CODE_LOC
## source "$CODE_LOC/ZSH/zshrc-loader.sh"
##
##NOTE: in bash/ksh/zsh export CODE_LOC="/mnt/e/CODE would also be valid but not in POSIX wehre the assignement and export must be seperate"

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
