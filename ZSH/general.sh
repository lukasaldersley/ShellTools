#!/bin/false
# shellcheck shell=bash
#i am using bash here since I need functions and path expansion which are in bash/zsh but not dash/sh or POSIX compatible

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