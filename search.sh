#!/bin/sh
currDir=$(pwd)
cd "${2:-$currDir}" || exit
echo "searching files with names matching *$1* in path $(pwd)"
find "$(pwd)" -type f,d,p,l,s -iname "*$1*" 2>&1|grep -v "Permission denied"|grep -i --color=auto "$1"
cd "$currDir" || exit
#    -type c
#        File is of type c:
#        b      block (buffered) special
#        c      character (unbuffered) special
#        d      directory
#        p      named pipe (FIFO)
#        f      regular file
#        l      symbolic link; this is never true if the -L option or the -follow option is in effect, unless the symbolic link is broken.  If you want to search for symbolic links when -L is in effect, use -xtype.
#        s      socket
#        D      door (Solaris)
#        To search for more than one type at once, you can supply the combined list of type letters separated by a comma `,' (GNU extension).
#UNUSUAL FILENAMES
#    Many of the actions of find result in the printing of data which is under the control of other users.  This includes file names, sizes, modification times and so forth.  File names are a potential problem since they can con?
#    tain any character except `\0' and `/'.  Unusual characters in file names can do unexpected and often undesirable things to your terminal (for example, changing the settings of your function keys on some terminals).  Unusual
#    characters are handled differently by various actions, as described below.
#
#    -print0, -fprint0
#        Always print the exact filename, unchanged, even if the output is going to a terminal.
#    -ls, -fls
#        Unusual  characters  are  always  escaped.  White space, backslash, and double quote characters are printed using C-style escaping (for example `\f', `\"').  Other unusual characters are printed using an octal escape.
#        Other printable characters (for -ls and -fls these are the characters between octal 041 and 0176) are printed as-is.
#    -iname pattern
#        Like -name, but the match is case insensitive.  For example, the patterns `fo*' and `F??' match the file names `Foo', `FOO', `foo', `fOo', etc.  The pattern `*foo*` will also match a file called '.foobar'.
#    -name pattern
#        Base of file name (the path with the leading directories removed) matches shell pattern pattern.  Because the leading directories are removed, the file names considered for a match with  -name  will  never  include  a
#        slash,  so  `-name a/b' will never match anything (you probably need to use -path instead).  A warning is issued if you try to do this, unless the environment variable POSIXLY_CORRECT is set.  The metacharacters (`*',
#        `?', and `[]') match a `.' at the start of the base name (this is a change in findutils-4.2.2; see section STANDARDS CONFORMANCE below).  To ignore a directory and the files under it, use -prune rather  than  checking
#        every file in the tree; see an example in the description of that action.  Braces are not recognised as being special, despite the fact that some shells including Bash imbue braces with a special meaning in shell pat?
#        terns.  The filename matching is performed with the use of the fnmatch(3) library function.  Don't forget to enclose the pattern in quotes in order to protect it from expansion by the shell.