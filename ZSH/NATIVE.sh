#!/bin/false
#shellcheck shell=sh
printf "\e[38;5;240mSourcing %s\e[0m\n" "$0"
#printf "\e[38;5;240mSourcing Native Linux/Debian-esque specifics\e[0m\n"

alias stopxfce4="xfce4-session-logout -l" # if the system was started in CLI mode and elevated to GUI with 'startxfce4' this brings it back down to CLI
alias bios="systemctl reboot --firmware-setup"
