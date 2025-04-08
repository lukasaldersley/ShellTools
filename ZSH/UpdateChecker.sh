#!/bin/sh

if [ ! -e "$ST_CFG/.lastfetch" ] || [ "$(($(date -u +%s)-$(cat "$ST_CFG/.lastfetch")))" -gt 40320 ]; then
	echo "[INFO] Last known successfull fetch of ShellTools was more than four weeks ago. Checking now."
	if git -C "$ST_SRC" fetch 1>/dev/null 2>&1 ; then
		date +%s > "$ST_CFG/.lastfetch"
	fi
fi
