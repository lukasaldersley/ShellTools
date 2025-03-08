#!/bin/sh

#if this is running under WSL1 on ubuntu there will be a snap applicaton, but one that just prints a message stating incompatibility and exits.
#since snap may as well not be installed, seing as it isn't functional, just quit early
if ! command -v snap > /dev/null 2>&1 || [ "$WSL_VERSION" -eq 1 ] > /dev/null 2>&1 ; then
	exit 1
fi
printf "you have %s snaps installed (last updated %s)\n" "$(find /var/lib/snapd/snaps -type f -name "*.snap" | wc -l)" "$(snap refresh --time | grep last | cut -c7-)"
