#!/bin/sh

if [ ! -e "$ST_CFG/.lastfetch" ]; then
	# file didn't exist -> create new one with a unix timestamp of 0 -> do check now
	echo "0" > "$ST_CFG/.lastfetch"
fi
if [ "$(($(date -u +%s)-$(cat "$ST_CFG/.lastfetch")))" -gt 40320 ]; then
	printf "[INFO] Last known successfull fetch of ShellTools was more than four weeks ago (on %s). Checking now.\n" "$(date --date="@$(cat "$ST_CFG"/.lastfetch)" +'%Y-%m-%dT%H:%M:%S%z (%Z)')"
	if git -C "$ST_SRC" fetch 1>/dev/null 2>&1 ; then
		date +%s > "$ST_CFG/.lastfetch"
	fi
fi
