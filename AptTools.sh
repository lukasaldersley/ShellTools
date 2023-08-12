#!/bin/sh

getopt --test > /dev/null
if [ $? -ne 4 ]; then
	echo "getopt-test failed -> probs invalid env"
	exit 1
fi

LONGOPTS=unattended,dist-upgrade,full-update,check
OPTIONS=udfc

# -pass arguments only via   -- "$@"   to separate them correctly
PARSED=$(getopt --options=$OPTIONS --longoptions=$LONGOPTS --name "$0" -- "$@")
rc=$?
if [ $rc -ne 0 ]; then
	echo "getopt returnded nonzero result: $rc"
	# e.g. return value is 1
	#  then getopt has complained about wrong arguments to stdout
	exit 2
fi
#echo "<$PARSED>"
#this replaces the script's inputs as if it was called with what getopts has parsed
eval set -- "$PARSED"

dryrun=0
nolock=0
admin=0
doUpdate=0
distUpgrade=0
unattend=0
full=0
# now enjoy the options in order and nicely split until we see --
#note: shift removes the first argument and thereby kind of shifts the arguments to the left
while true; do
	case "$1" in
		-u|--unattended)
			unattend=1
			dryrun=1
			nolock=1
			admin=0
			doUpdate=0
			shift
			;;
		-d|--dist-upgrade)
			distUpgrade=1
			shift
			;;
		-c|--check)
			admin=1
			dryrun=1
			nolock=0
			doUpdate=1
			shift
			;;
		-f|--full-update)
			full=1
			distUpgrade=1
			admin=1
			doUpdate=1
			nolock=0
			shift
			;;
		--)
			shift
			break
			;;
		*)
			echo "Programming error"
			exit 3
			;;
	esac
done

if [ $unattend -ne 0 ] && [ $full -ne 0 ]; then
	echo "--full-update and --unattend are mutually exclusive"
	exit 1
fi

if [ $admin -eq 0 ] && [ "$(whoami)" != "root" ] && [ $dryrun -eq 0 ]; then
	echo "no admin provided -> setting to dryrun"
	dryrun=1
fi

cmd="apt-get "
if [ $admin -ne 0 ]; then
	cmd="sudo $cmd"
	if [ $doUpdate -ne 0 ]; then
		cmd="sudo apt-get update && $cmd"
	fi
fi
if [ $dryrun -ne 0 ]; then
	cmd="${cmd}--dry-run "
fi
if [ $nolock -ne 0 ];then
	cmd="${cmd}-o Debug::NoLocking=true "
fi
if [ $distUpgrade -ne 0 ]; then
	cmd="${cmd}dist-"
fi
cmd="${cmd}upgrade"
#echo "$cmd"
eval "LANG=C.UTF-8 LANGUAGE="" $cmd" | sed -nE 's~([0-9]+) upgraded.*and ([0-9]+) not upgraded.+~There are \1 packages to be updated and \2 for dist-upgrade ~p'|tr -d '\n'
#according to https://serverfault.com/questions/20747/find-last-time-update-was-performed-with-apt-get /var/lib/apt/lists/partial/ is the one I want to go with to check when the last update was
#
# shellcheck disable=SC2010 #reason for shellcheck disable is: this is one of the cases where ls's sorting (-t, sort by time) is important and that's fine according to shellcheck.net
t2=$(stat -c %Y "/var/lib/apt/lists/$(ls -lt /var/lib/apt/lists --hide "lock" |grep "^-" -m1|awk '{print $NF}')")
timestamp="$(stat -c %Y /var/lib/apt/lists/partial/)"
now=$(date +%s)
diff=$((now-timestamp))
d2=$((now-t2))

printf "(Archives are "
if [ $d2 -lt 60 ]; then
	printf "%s seconds" "$d2"
elif [ $d2 -lt 3600 ]; then
	printf "%s minutes" "$((d2 / 60))"
elif [ $d2 -lt 86400 ]; then
	printf "%s hours" "$((d2 / 3600))"
else
	printf "%s days" "$((d2 / 86400))"
fi

printf " old, fetched "

if [ $diff -lt 60 ]; then
	echo "$diff seconds ago)"
elif [ $diff -lt 3600 ]; then
	echo "$((diff / 60)) minutes ago)"
elif [ $diff -lt 86400 ]; then
	echo "$((diff / 3600)) hours ago)"
else
	echo "$((diff / 86400)) days ago)"
fi

if [ -f /var/run/reboot-required ]; then
	cat /var/run/reboot-required
fi
