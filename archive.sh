#!/bin/sh

getopt --test > /dev/null
if [ $? -ne 4 ]; then
	echo "getopt-test failed -> probs invalid env"
	exit 1
fi

LONGOPTS=pack,unpack,keep-source,verbose
OPTIONS=pukv

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

p=0 v=0 k=0 u=0
# now enjoy the options in order and nicely split until we see --
#note: shift removes the first argument and thereby kind of shifts the arguments to the left
while true; do
	case "$1" in
		-p|--pack)
			p=1
			shift
			;;
		-u|--unpack)
			u=1
			shift
			;;
		-k|--keep-source)
			k=1
			shift
			;;
		-v|--verbose)
			v=1
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

#pack XOR unpack must be specified
if ! [ $p -ne $u ] && { [ $p -ne 0 ] || [ $u -ne 0 ]; }; then
	echo "either -p or -u MUST be specified but NOT both"
	exit 1
fi

#count of arguments must be exactly one after the switched are removed
if [ $# -ne 1 ]; then
	echo "$0: A single input file or folder is required."
	exit 4
fi

#echo "pack: $p, unpack $u, keep: $k, verbose: $v, in: $1"
#create command string
cmd="tar"
if [ $v -ne 0 ] || [ $k -eq 0 ]; then
	#make verbose if requested, but also if the files are NOT KEPT, regardless of verbosity switch
	cmd="${cmd} -v"
fi
if [ $p -ne 0 ]; then
	cmd="${cmd} -zcf \"$1.tar.gz\" \"$1\""
else
	cmd="${cmd} -xf \"$1\""
fi
#echo "$cmd"
#check target existance and execute
if [ -f "$1" ] || { [ -d "$1" ] && [ $p -ne 0 ]; } ; then
	sh -c "$cmd"
	ret=$?
	#echo $ret
	#if command succeded, ckeck if deletion is requested
	if [ $ret -eq 0 ]; then
		if [ $k -eq 0 ]; then
			#if it's a folder, do rm -rf, else just do rm
			if [ $p -ne 0 ] && [ -d "$1" ]; then
				rm -rf "$1"
				#echo "delete rf"
				true
			else
				rm "$1"
				#echo "delete"
				true
			fi
		fi
	else
		echo "error in tar ($ret)"
		exit $ret
	fi
else
	echo "'$1' can't be found (or is of the wrong type), please check your input"
	exit 1
fi
exit

