#/*
TargetDir="$ST_CFG"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename $0 .c).elf"
printf "compiling $0 into $TargetDir/$TargetName"
gcc -O3 -std=c2x -Wall "$0" -o "$TargetDir/$TargetName"
printf " -> \e[32mDONE\e[0m($?)\n"
exit
*/
#include <stdio.h>

int main(int argc, char** argv) {
	printf("argc: %i\n", argc);
	for (int i = 0;i < argc;i++) {
		printf("arg %i:\t>%s<\n", i, argv[i]);
	}
}
