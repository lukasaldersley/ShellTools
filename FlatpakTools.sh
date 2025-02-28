#!/bin/sh

#check if flatpak exists, redirect output to void, exit if flatpak unavailable
if ! command -v flatpak > /dev/null 2>&1; then
	exit 1
fi
#there is a discussion here:
#fastfetch counts the number of folders in /var/lib/flatpak/{app,runtime} (those in app being the user facing ones, the others being runtime libs). on my system fastfetch came to 19 packages:
#	/var/lib/flatpak/app:
#	total 36K
#	4,0K drwxr-xr-x 9 root root 4,0K 2025-02-22 17:43:56 .
#	4,0K drwxr-xr-x 8 root root 4,0K 2025-02-28 19:03:39 ..
#	4,0K drwxr-xr-x 3 root root 4,0K 2025-02-22 17:07:25 com.github.marinm.songrec
#	4,0K drwxr-xr-x 3 root root 4,0K 2025-02-22 17:07:34 com.github.Matoking.protontricks
#	4,0K drwxr-xr-x 3 root root 4,0K 2025-02-22 17:07:17 com.github.tchx84.Flatseal
#	4,0K drwxr-xr-x 3 root root 4,0K 2025-02-22 17:05:31 com.prusa3d.PrusaSlicer
#	4,0K drwxr-xr-x 3 root root 4,0K 2025-02-22 17:07:33 com.valvesoftware.Steam
#	4,0K drwxr-xr-x 3 root root 4,0K 2025-02-22 17:43:56 org.mozilla.firefox
#	4,0K drwxr-xr-x 3 root root 4,0K 2025-02-22 17:43:24 org.mozilla.Thunderbird
#
#	/var/lib/flatpak/runtime:
#	total 56K
#	4,0K drwxr-xr-x 14 root root 4,0K 2025-02-22 17:43:52 .
#	4,0K drwxr-xr-x  8 root root 4,0K 2025-02-28 19:03:39 ..
#	4,0K drwxr-xr-x  3 root root 4,0K 2025-02-22 17:05:13 com.prusa3d.PrusaSlicer.Locale
#	4,0K drwxr-xr-x  3 root root 4,0K 2025-02-22 17:07:24 org.freedesktop.Platform
#	4,0K drwxr-xr-x  3 root root 4,0K 2025-02-22 17:07:02 org.freedesktop.Platform.Compat.i386
#	4,0K drwxr-xr-x  3 root root 4,0K 2025-02-22 17:07:13 org.freedesktop.Platform.GL32.default
#	4,0K drwxr-xr-x  3 root root 4,0K 2025-02-22 17:04:31 org.freedesktop.Platform.GL.default
#	4,0K drwxr-xr-x  3 root root 4,0K 2025-02-22 17:07:15 org.freedesktop.Platform.Locale
#	4,0K drwxr-xr-x  3 root root 4,0K 2025-02-22 17:04:33 org.freedesktop.Platform.openh264
#	4,0K drwxr-xr-x  3 root root 4,0K 2025-02-22 17:05:27 org.gnome.Platform
#	4,0K drwxr-xr-x  3 root root 4,0K 2025-02-22 17:04:35 org.gnome.Platform.Locale
#	4,0K drwxr-xr-x  3 root root 4,0K 2025-02-22 17:04:35 org.gtk.Gtk3theme.Breeze
#	4,0K drwxr-xr-x  3 root root 4,0K 2025-02-22 17:43:52 org.mozilla.firefox.Locale
#	4,0K drwxr-xr-x  3 root root 4,0K 2025-02-22 17:43:20 org.mozilla.Thunderbird.Locale
#
#However flatpak list gives a different picture:
#	flatpak list
#	Name                                               Application ID                                     Version                               Branch                  Installation
#	Protontricks                                       com.github.Matoking.protontricks                   1.12.0                                stable                  system
#	SongRec                                            com.github.marinm.songrec                          0.4.3                                 stable                  system
#	Flatseal                                           com.github.tchx84.Flatseal                         2.3.0                                 stable                  system
#	PrusaSlicer                                        com.prusa3d.PrusaSlicer                            2.9.0                                 stable                  system
#	Steam                                              com.valvesoftware.Steam                            1.0.0.81                              stable                  system
#	Freedesktop Platform                               org.freedesktop.Platform                           freedesktop-sdk-23.08.28              23.08                   system
#	Freedesktop Platform                               org.freedesktop.Platform                           freedesktop-sdk-24.08.12              24.08                   system
#	i386                                               org.freedesktop.Platform.Compat.i386                                                     24.08                   system
#	Mesa                                               org.freedesktop.Platform.GL.default                24.3.4                                23.08                   system
#	Mesa (Extra)                                       org.freedesktop.Platform.GL.default                24.3.4                                23.08-extra             system
#	Mesa                                               org.freedesktop.Platform.GL.default                24.3.4                                24.08                   system
#	Mesa (Extra)                                       org.freedesktop.Platform.GL.default                24.3.4                                24.08extra              system
#	Mesa                                               org.freedesktop.Platform.GL32.default              24.3.4                                24.08                   system
#	Mesa (Extra)                                       org.freedesktop.Platform.GL32.default              24.3.4                                24.08extra              system
#	openh264                                           org.freedesktop.Platform.openh264                  2.1.0                                 2.2.0                   system
#	openh264                                           org.freedesktop.Platform.openh264                  2.4.1                                 2.4.1                   system
#	GNOME Application Platform version 47              org.gnome.Platform                                                                       47                      system
#	Breeze GTK theme                                   org.gtk.Gtk3theme.Breeze                           6.3.2                                 3.22                    system
#	Thunderbird                                        org.mozilla.Thunderbird                            128.7.1esr                            stable                  system
#	Firefox                                            org.mozilla.firefox                                135.0.1                               stable                  system
#
#flatpak list shows different branches of installed packages as seperate, but hides any *.Locale packages (I assume the .Locale packages just provide translations)
#for now I am going to go with flatpak list's interpretation but add the convenience feature of showing how many user facing apps there are by parsing /var/lib/flatpak/app as well

printf "Out of %s installed flatpaks (%s apps), there are %s to be updated\n" "$(flatpak list | tail -n+1 | wc -l)" "$(ls /var/lib/flatpak/app | wc -l)" "$(flatpak remote-ls --updates | tail -n+1 | wc -l)"
