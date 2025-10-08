#/*
TargetDir="$ST_CFG"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename "$0" .c).elf"
printf "compiling %s into %s/%s" "$0" "$TargetDir" "$TargetName"
gcc -O3 -std=c2x -Wall "$(realpath "$(dirname "$0")")"/commons.c "$0" -o "$TargetDir/$TargetName" "$@"
#I WANT to be able to do things like ./shelltoolsmain.c -DPROFILING to add the compiler flag profiling but ALSO stuff like ./shelltoolsmain "-DDEBUG -DPROFILING" to add both profiling and debug
printf " -> \e[32mDONE\e[0m(%s)\n" $?
exit
*/

#include "commons.h" // for ExecuteProcess_alloc, TerminateStrOn

#include <stdio.h> // for printf
#include <stdlib.h> // for free

int main(int argc, char** argv) {
	char* result = ExecuteProcess_alloc("pwd");
	TerminateStrOn(result, "\n");
	printf("PWD: %s\n", result);
	free(result);
	printf("argc: %i\n", argc);
	for (int i = 0; i < argc; i++) {
		printf("arg %i:\t>%s<\n", i, *(argv + i));
	}
	printf("\n");
	return 0;
}
