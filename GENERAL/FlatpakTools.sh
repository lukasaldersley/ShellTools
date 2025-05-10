#!/bin/sh

#check if flatpak exists, redirect output to void, exit if flatpak unavailable
if ! command -v flatpak > /dev/null 2>&1; then
	exit 1
fi
#there is a discussion here:
#fastfetch counts the number of folders in /var/lib/flatpak/{app,runtime} (those in app being the user facing ones, the others being runtime libs).
#That will count Localisation as seperate applications, however different branches of the same package (relevant for runtime libraries) count as the same package
#flatpak list shows different branches of installed packages as seperate, but hides any *.Locale packages (I assume the .Locale packages just provide translations)
#for now I am going to go with flatpak list's interpretation but add the convenience feature of showing how many user facing apps there are by parsing /var/lib/flatpak/app as well

# Flatpak remote-ls --updates uses the same logic as flatpak list, the confirmation for flatpak update (the actual update command does show the locale packages)

printf "Out of %s installed flatpaks (%s apps) [flatpak list interpretation, Ignoring Localisation], there are %s to be updated\n" "$(flatpak list | tail -n+1 | wc -l)" "$(find /var/lib/flatpak/app -maxdepth 1 -mindepth 1 -type d | wc -l)" "$(flatpak remote-ls --updates | tail -n+1 | wc -l)"
