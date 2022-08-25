#!/bin/sh
WinUser=$(cmd.exe /c "echo %USERNAME%"|rev|cut -c2-|rev)
#the whole crap about |rev|cut -c2-|rev is to remove the \r\n Windows' echo adds
#the reason for the double rev is that cut only has functionality to return everything between two markers of from a marker to the end of line or to a fixed index.
#Since the username can be any length and I always want to remove the last two chars I need to reverse the string, and keep everything from index 2 up to
#the end of line, so that when I reverse it again to get back the original order I actually have everything from the beginning and have removed only the last two.
rm -rf "/mnt/c/Users/$WinUser/.ssh"
cp -r ~/.ssh "/mnt/c/Users/$WinUser/.ssh"
