#!/bin/bash

#abbreviated info from https://blog.carbonfive.com/writing-zsh-themes-a-quickref/
#PROMPT='whatever is in here is on the left of the text input prompt'
#RPROMPT='whatever is in here is in the right side of the screen of the prompt'
#anyhing to be passed into PROMPT will be interpreted by the shell and should NOT be double quoted.
#colours are set INLINE using %{$fg[...]%} where ... is a web-safe colour of black/blue/red/magenta/green/cyan/yellow/white
#to use a different colour than the eight defined websafe ones use FG instead of fg and insert the colour code obtained from the spectrum_ls command
#the same applies to Background colours, just use BG/bg instead of FG/fg
#there is a shorthand for both: %F{...} for text(foreground) colour and %K{...} for background (same for text and number, no more case difference)
#to reset a colour back to default use %{$reset_color%} (restets both text and background) or the shorthand of %f for text and %k for background
#useful powerline (eg https://github.com/powerline/fonts) font characters: \u2718 is errorX (like for unclean git)
#prompt variables: %n -> current user   |   %0 -> pwd   %1\ -> current dir   %2 -> current dir + parent dir etc. (if used wth a ~ eg %0~ it'd abbreviate paths with ~)
#%T gives time as hh:mm   %* gives time as hh:mm:ss   %D{...} is custom strftime format
#%? returns status code of last command
#conditional syntax: %(TestChar.TrueVal.FalseVal) (for testChar doc see https://zsh.sourceforge.io/Doc/Release/Prompt-Expansion.html#Conditional-Substrings-in-Prompts)
#conditional: the seperator doesn't need to be . it is arbitrary
#todo add a line at the bottom of the screen with status
#todo add info what modules are loaded (WSL/BMW/General/others...)

function backgroundTasks (){
    #this calls `jobs` and uses `sed` to un following regex: ^\[(\d+)\]\s+([+|-]?)\s*(\w).+$
    #(basically takes the number in square brackets, an optional + or - and the first letter of the status as capture groups and prints them in the prder 1 3 2)
    #then tr is used to get rid of linebreaks and shove everything to uppercase. finally rev|cut -c2-|rev chops trailing whitespace off
    local background_task_info
    background_task_info="$(jobs|sed 's/^\[\([0-9]\+\)\][^a-zA-Z+\-]*\([+\-]\?\)[^a-zA-Z+\-]*\(.\).*$/\1\3\2/'|tr 'a-z\n' 'A-Z '|rev|cut -c2- |rev)"
    if [ ${#background_task_info} -eq "0" ]; then
        echo ""
    else
        echo " [$background_task_info]"
    fi
}
local user_at_host='%F{010}%n%F{007}@%F{033}%m%f'               # 'user@host'
local last_command_indicator='%B%(?:%F{green}:%F{red})[%?]➜%f%b'     # '[returncode]➜'
local WorkingDirectory='%F{cyan}%B%c%b%f'                           # 'directory'
#color palette can be found by executing command spectrum_ls
PROMPT='${user_at_host} $WorkingDirectory $(git_prompt_info)$(gitRemoteResolv)$last_command_indicator '
RPROMPT='%*$(backgroundTasks)'
#COMPLETION_WAITING_DOTS="%F{yellow}waiting...%f"

function gitRemoteResolv (){
    #local repoinfo="$(git remote -v|head -1|sed 's/^.*\([ssh|http.]\?\).*@\([a-zA-Z0-9\.\-\_]*\).*$/\1:\2/')"
    local fulltextremote
    fulltextremote="$(git ls-remote --get-url origin)"
    if [ "$fulltextremote" != 'origin' ]; then
        local localrepos
        localrepos="$(realpath -q -s --relative-to=$(pwd) $fulltextremote)"
        local remoteRepos
        remoteRepos="$(sed -n 's|^\([a-zA-Z0-9]\+\)://\([a-zA-Z0-9]\+@\)\?\([a-zA-Z0-9._\-]\+\).*$|\1:\3|p' <<< $fulltextremote)"
        echo "%F{001}from %F{009}$localrepos$remoteRepos %f"
    else
        echo ""
    fi
}

ZSH_THEME_GIT_PROMPT_PREFIX="%F{001}git:(%F{009}"
ZSH_THEME_GIT_PROMPT_SUFFIX="%f "
ZSH_THEME_GIT_PROMPT_DIRTY="%F{001}) %F{011}\u2573"
ZSH_THEME_GIT_PROMPT_CLEAN="%F{001})"
#$(git_prompt_info) is built as follows: PREFIX+branch+[DIRTY|CLEAN]+SUFFIX  //Dirty/Clean je nachdem was zutrifft (there's also extra stuff in OhMyZsh)
