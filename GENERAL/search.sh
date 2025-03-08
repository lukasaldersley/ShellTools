#!/bin/sh

getopt --test > /dev/null
if [ $? -ne 4 ]; then
	echo "getopt-test failed -> probs invalid env"
	exit 1
fi

LONGOPTS=pattern:,content
OPTIONS=p:c

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

pattern=""
c=0
# now enjoy the options in order and nicely split until we see --
while true; do
	case "$1" in
		-c|--content)
			c=1
			shift
			;;
		-p|--pattern)
			if [ -z "$pattern" ]; then
				pattern=$2
			else
				echo "error, only 1 pattern"
			fi
			shift 2
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
if [ -z "$pattern" ]; then
	echo "requires exactly one pattern"
fi
echo "These folders will be processed: $*"
if [ $# -eq 0 ]; then
#if no folder given, use ./ as default by inserting it as if it was the last argument
	set -- "./"
fi
if [ $# -ge 1 ]; then
	currDir=$(pwd)
	for arg; do
		cd "${arg}" || exit
		echo "--> searching files with names matching *$pattern* in path $(pwd)"
		{ find "$(pwd)" -type f,d,p,l,s -iname "*$pattern*" 2>&1 1>&3 3>&- | grep -v "Permission denied"; } 3>&1 1>&2 | grep -i --color=yes "$pattern"

		if [ $c -ne 0 ]; then
			echo "--> searching file contents matching *$pattern* in $(pwd)"
			{ grep --color=yes -iRnH -e "$pattern" "$(pwd)" 2>&1 1>&3 3>&- | grep -v "Permission denied" | grep -v "No such device or address" | grep -v "No such file or directory"; } 3>&1 1>&2
		fi
		cd "$currDir" || exit
	done
fi
exit

##explanation of the terms
##
##
#https://stackoverflow.com/questions/9112979/pipe-stdout-and-stderr-to-two-different-processes-in-shell-script/31151808#31151808
##
##
#FIND expression
#	-type c
#		File is of type c:
#		b	block (buffered) special
#		c	character (unbuffered) special
#		d	directory
#		p	named pipe (FIFO)
#		f	regular file
#		l	symbolic link; this is never true if the -L option or the -follow option is in effect, unless the symbolic link is broken.  If you want to search for symbolic links when -L is in effect, use -xtype.
#		s	socket
#		D	door (Solaris)
#		To search for more than one type at once, you can supply the combined list of type letters separated by a comma `,' (GNU extension).
#UNUSUAL FILENAMES
#	Many of the actions of find result in the printing of data which is under the control of other users.
#	This includes file names, sizes, modification times and so forth.
#	File names are a potential problem since they can contain any character except `\0' and `/'.
#	Unusual characters in file names can do unexpected and often undesirable things to your terminal (for example, changing the settings of your function keys on some terminals).
#	Unusual characters are handled differently by various actions, as described below.
#
#	-print0, -fprint0
#		Always print the exact filename, unchanged, even if the output is going to a terminal.
#	-ls, -fls
#		Unusual  characters  are  always  escaped.
#		White space, backslash, and double quote characters are printed using C-style escaping (for example `\f', `\"').
#		Other unusual characters are printed using an octal escape.
#		Other printable characters (for -ls and -fls these are the characters between octal 041 and 0176) are printed as-is.
#	-iname pattern
#		Like -name, but the match is case insensitive.
#		For example, the patterns `fo*' and `F??' match the file names `Foo', `FOO', `foo', `fOo', etc.
#		The pattern `*foo*` will also match a file called '.foobar'.
#	-name pattern
#		Base of file name (the path with the leading directories removed) matches shell pattern pattern.
#		Because the leading directories are removed, the file names considered for a match with  -name  will  never  include  a slash,  so  `-name a/b' will never match anything (you probably need to use -path instead).
#		A warning is issued if you try to do this, unless the environment variable POSIXLY_CORRECT is set.
#		The metacharacters (`*', `?', and `[]') match a `.' at the start of the base name (this is a change in findutils-4.2.2; see section STANDARDS CONFORMANCE below).
#		To ignore a directory and the files under it, use -prune rather  than  checking every file in the tree; see an example in the description of that action.
#		Braces are not recognised as being special, despite the fact that some shells including Bash imbue braces with a special meaning in shell patterns.
#		The filename matching is performed with the use of the fnmatch(3) library function.
#		Don't forget to enclose the pattern in quotes in order to protect it from expansion by the shell.
##
##
#GREP expression
#grep [OPTION...] -e PATTERNS ... [FILE...]
#A FILE of "-" stands for standard input.  If no FILE is given, recursive searches examine the working directory, and nonrecursive searches read standard input.
# -i, --ignore-case					Ignore case distinctions in patterns and input data, so that characters that differ only in case match each other.
# -R, --dereference-recursive		Read all files under each directory, recursively.  Follow all symbolic links, unlike -r.
# -n, --line-number					Prefix each line of output with the 1-based line number within its input file.
# -H, --with-filename				Print the file name for each match.  This is the default when there is more than one file to search.  This is a GNU extension.
# -e PATTERNS, --regexp=PATTERNS	Use PATTERNS as the patterns.  If this option is used multiple times or is combined with the -f (--file) option, search for all patterns given.  This option can be used to protect a pattern beginning with "-".
