#/*
TargetDir="$ST_CFG"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename "$0" .c).elf"
printf "compiling %s into $TargetDir/$TargetName" "$0"
#shellcheck disable=SC2086
gcc -O3 -std=c2x -Wall "$0" -o "$TargetDir/$TargetName" $1
#the fact I DIDN'T add quotations to the $1 above means I WANT word splitting to happen.
#I WANT to be able to do things like ./repotools.c -DPROFILING to add the compiler flag profiling but ALSO stuff like ./repotools "-DDEBUG -DPROFILING" to add both profiling and debug
printf " -> \e[32mDONE\e[0m(%s)\n" $?
exit
*/
#include <stdio.h>

int main(int argc, char** argv) {
	printf("argc: %i\n", argc);
	for (int i = 0;i < argc;i++) {
		printf("arg %i:\t>%s<\n", i, *(argv + i));
	}
}
