#!/bin/sh

## this script must be sourced in zshrc as follows (obviously adjust CODE_LOC):
## 
## CODE_LOC="/mnt/e/CODE"
## export CODE_LOC
## source "$CODE_LOC/BAT_VBS/zshrc-modules.sh"

#the . somescript is equivalent to source somescript but is portable since it's POSIX compliant
# shellcheck source=./zshrc_modules/general.sh
. "$CODE_LOC/BAT_VBS/zshrc_modules/general.sh"

#this is a command that's EXPECTED to fail every time this is run on a non-WSL installation, so the stderr redirect and masking of the return code are intentional
{ WSL_VERSION="$(/mnt/c/Windows/System32/wsl.exe -l -v | iconv -f unicode|grep -E "\b$WSL_DISTRO_NAME\s+Running" | awk '{print $NF}')"; } 2> /dev/null || true
if [ -z "$WSL_VERSION" ]; then
    echo "native -> don't source WSL"
else
    #echo "wsl: $WSL_VERSION"
    # shellcheck source=./zshrc_modules/wsl.sh
    . "$CODE_LOC/BAT_VBS/zshrc_modules/wsl.sh"
fi