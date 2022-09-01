#!/bin/bash

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
    if [ -r ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme ]; then
        rm ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme
    fi
    cp "$CODE_LOC/BAT_VBS/lukasaldersley.zsh-theme" ~/.oh-my-zsh/custom/themes/lukasaldersley.zsh-theme
}