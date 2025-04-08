#!/bin/sh

if ! command -v apt > /dev/null 2>&1; then
	exit 1
fi

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
___apt_tempfile01___="$(mktemp)"
if [ $full -ne 0 ];then
	bash -ic "$cmd"
	eval "$0 -u"
	exit
else
	#echo "$cmd"
	printf "Out of %s installed dpkg packages, " "$(dpkg -l | grep -c '^.i')"
	eval "LANG=C.UTF-8 LANGUAGE="" $cmd">"$___apt_tempfile01___"
	sed -nE 's~([0-9]+) upgraded.*and ([0-9]+) not upgraded.+~there are \1 packages to be updated and \2 held back ~p' < "$___apt_tempfile01___"|tr -d '\n'
fi

___apt_tempfile02___="$(mktemp)"
# shellcheck disable=SC2046 # SC2046 is double quote to prevent word splitting. in this case I WANT word splitting, see SC2046 wiki for another example
#apt-get used to list phased update packages simply as held back (as seen in ubuntu 22.04/apt-get 2.4.13), in later releases (such as kubuntu 24.10/apt-get 2.9.8 and later) those got their own output line with "deferred due to phasing". this code has been adapted to work with both
apt show -a $(tr '\n' '|' <"$___apt_tempfile01___"|sed 's~|  ~ ~g'|grep -Eo -e 'have been kept back:[^\|]+' -e 'deferred due to phasing:[^\|]+'|sed -E 's~.+: (.+)~\1~'|tr '\n' ' ') 2>&1 | grep Phased | sort -r > "$___apt_tempfile02___";
___apt_tempvar01___="$(wc -l <"$___apt_tempfile02___")";
printf "(%s of those are held by phased update" "$___apt_tempvar01___";
if [ "$___apt_tempvar01___" -ne 0 ]; then
	printf ": "
	uniq -c <"$___apt_tempfile02___" | sed -nE 's~ *([0-9]+)[^:]+: ([0-9]+)~\1\x1b[38;5;242m@\2%\x1b[0m ~p'|tr -d '\n'|rev|cut -c2-|rev|tr -d '\n'
fi
printf ") ";
rm "$___apt_tempfile01___"
rm "$___apt_tempfile02___"
unset ___apt_tempfile01___
unset ___apt_tempfile02___
unset ___apt_tempvar01___
#There are 0 packages to be updated and 7 held back {7 Packages held by Staged Update: 2@0% 2@80% 3@70%} (Archives are 1 minutes old, fetched 1 minutes ago)
#when a package reaches 100% staged update it's rolled out globally until then based on machine ID and package name I may or may not get it yet

#according to https://serverfault.com/questions/20747/find-last-time-update-was-performed-with-apt-get /var/lib/apt/lists/partial/ is the one I want to go with to check when the last update was
#
# shellcheck disable=SC2010 #reason for shellcheck disable is: this is one of the cases where ls's sorting (-t, sort by time) is important and that's fine according to shellcheck.net
t2=$(stat -c %Y "/var/lib/apt/lists/$(ls -lt /var/lib/apt/lists --hide "lock" |grep "^-" -m1|awk '{print $NF}')")
timestamp="$(stat -c %Y /var/lib/apt/lists/partial/)"
now=$(date +%s)
diff=$((now-timestamp))
d2=$((now-t2))

if [ "$(tput cols)" -lt 140 ]; then
	printf "\n"
fi

printf "[Archives are "
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
	printf "%s seconds" "$diff"
elif [ $diff -lt 3600 ]; then
	printf "%s minutes" "$((diff / 60))"
elif [ $diff -lt 86400 ]; then
	printf "%s hours" "$((diff / 3600))"
else
	printf "%s days" "$((diff / 86400))"
fi
echo " ago]"

if [ -f /var/run/reboot-required ]; then
	cat /var/run/reboot-required
fi
if [ -f /var/run/reboot-required.pkgs ]; then
	echo "Reboot is required to apply updates for following packages:"
	sort < /var/run/reboot-required.pkgs | uniq
fi
