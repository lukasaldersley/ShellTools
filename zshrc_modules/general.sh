#!/bin/false
# shellcheck shell=bash
#i am using bash here since I need functions and path expansion which are in bash/zsh but not dash/sh or POSIX compatible

export EDITOR=nano
#the hash -d something=other is a bit like ~ means /mnt/user/home; ~something now means other
hash -d CODE="$CODE_LOC"
hash -d code="$CODE_LOC"

#script invocations
alias pack='$CODE_LOC/BAT_VBS/packAndDeleteFolder.sh'
alias unpack='$CODE_LOC/BAT_VBS/unpackAndDeleteArchive.sh'
alias search='$CODE_LOC/BAT_VBS/search.sh'
alias contentsearch='$CODE_LOC/BAT_VBS/contentsearch.sh'

#general shorthands
alias zshrc="nano ~/.zshrc"
alias cls=clear
alias gitlog="git log --branches --remotes --tags --graph --oneline --decorate --notes HEAD"
alias qqq=exit
alias dotnet=~/.dotnet/dotnet

function updateTheme () {
    if [ -w ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme ]; then #if file exists and write permission is granted
        rm ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme
    fi
    cp "$CODE_LOC/BAT_VBS/lukasaldersley.zsh-theme" ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme
    omz reload
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