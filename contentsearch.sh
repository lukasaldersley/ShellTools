#!/bin/sh
initDir=$(pwd)
cd "${2:-$initDir}" || exit
echo "searching files and file contents matching $1 in $(pwd)"
#explanantion of below grep statement:
#grep [OPTION...] -e PATTERNS ... [FILE...]
#A FILE of "-" stands for standard input.  If no FILE is given, recursive searches examine the working directory, and nonrecursive searches read standard input.
# -i, --ignore-case                 Ignore case distinctions in patterns and input data, so that characters that differ only in case match each other.
# -R, --dereference-recursive       Read all files under each directory, recursively.  Follow all symbolic links, unlike -r.
# -n, --line-number                 Prefix each line of output with the 1-based line number within its input file.
# -H, --with-filename               Print the file name for each match.  This is the default when there is more than one file to search.  This is a GNU extension.
# -e PATTERNS, --regexp=PATTERNS    Use PATTERNS as the patterns.  If this option is used multiple times or is combined with the -f (--file) option, search for all patterns given.  This option can be used to protect a pattern beginning with “-”.
find "$(pwd)" -type f,d,p,l,s -iname "*$1*" 2>&1|grep -v "Permission denied"|grep -i --color=auto "$1"
#the find expression here is taken straight from search.sh
grep --color=auto -iRnH -e "$1" "$(pwd)"

cd "$initDir" || exit
