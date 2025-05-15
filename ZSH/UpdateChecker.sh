#!/bin/sh

if [ ! -e "$ST_CFG/.lastfetch" ]; then
	# file didn't exist -> create new one with a unix timestamp of 0 -> do check now
	echo "0" > "$ST_CFG/.lastfetch"
fi

ST_UPDATE_INTERVAL_DAYS=${ST_UPDATE_INTERVAL_DAYS:-14}
ST_UpdateTargetDiff=$((ST_UPDATE_INTERVAL_DAYS * 60 * 60 * 24))
ST_DateNow=$(date -u +%s)
ST_DateLast=$(cat "$ST_CFG/.lastfetch")
ST_diff=$((ST_DateNow-ST_DateLast))
unset ST_DateNow
unset ST_DateNow
if [ $ST_diff -gt $ST_UpdateTargetDiff ]; then
	printf "[INFO] Last known successfull fetch of ShellTools was more than %s days ago (on %s). Checking now.\n" "$ST_UPDATE_INTERVAL_DAYS" "$(date --date="@$(cat "$ST_CFG"/.lastfetch)" +'%Y-%m-%dT%H:%M:%S%z (%Z)')"
	if git -C "$ST_SRC" fetch 1>/dev/null 2>&1 ; then
		date +%s > "$ST_CFG/.lastfetch"
	fi
fi
unset ST_diff
unset ST_UpdateTargetDiff
