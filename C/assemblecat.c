#/*
TargetDir="$HOME/.shelltools"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename $0 .c).elf"
printf "compiling $0 into $TargetDir/$TargetName"
gcc -O3 -std=c2x -Wall "$(realpath "$(dirname "$0")")"/commons.c "$0" -o "$TargetDir/$TargetName"
printf " -> \e[32mDONE\e[0m($?)\n"
#eval "$HOME/$outname --equal=\" v\" AbBxB B Bdef efghiC v3.17 \" 3.17.0\" FX"
exit
*/

//add option to have chars be treated as equal such as --equal=" v" therfore 0x20 and v are to be the same (to allow matching up v3.17 with " 3.17.0")
#include "commons.h"
#include <string.h>

bool in_list(char c, char* eqlist) {
	if (eqlist == NULL) {
		return false;
	}
	char* lptr = eqlist;
	while (*lptr != 0x00) {
		if (*lptr == c) {
			return true;
		}
		lptr++;
	}
	return false;
}

char* reassemble(char* base, char* addon, char* eqlist) {
	int baselen = strlen(base);
	int addonlen = strlen(addon);
	char* res = malloc(sizeof(char) * (baselen + addonlen + 1));
	if (res == NULL) {
		return NULL;
	}
	int StartingPointInB = 0;
	//iterate through base and copy it to res.
	//while doing so, check where addon matches
	for (int b = 0; b < baselen;b++) {
		res[b] = base[b];
		int a_off = 0;
		while (base[b + a_off] != 0x00 && addon[a_off] != 0x00 && (base[b + a_off] == addon[a_off] || (in_list(base[b + a_off], eqlist) && in_list(addon[a_off], eqlist)))) {
			a_off++;
		}
		//at this point I have checked if addon matches base starting at b
		//if the match length (a_off) is greater than 0 addon is at least partially contained in base
		//if additionally the match length + the starting position is equal to the total base length, then the match spans to the end of base -> this position is the real starting point for concatenation
		if (a_off > 0 && a_off + b == baselen) {
			//printf("<%i/%i> in <%s>/<%s>\n", b, a_off, base, addon);
			StartingPointInB = a_off;
		}
	}
	int b = baselen;
	for (int a = StartingPointInB; a < addonlen; a++) {
		res[b] = addon[a];
		b++;
	}
	res[b] = 0x00;
	return res;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		fprintf(stderr, "you must supply at least one string to reassemble\n");
		return 1;
	}
	char* EQLIST = NULL;
	int startIndex = 1;
	if (StartsWith(argv[1], "--equal=")) {
		if (argc < 3) {
			fprintf(stderr, "you must supply at least one string to reassemble\n");
		}
		startIndex = 2;
		int offs = 8;
		EQLIST = malloc(sizeof(char) * strlen(argv[1]));
		if (EQLIST == NULL) {
			fprintf(stderr, "Not enough memory for EQ list -> attempting assembling WIHTOUT EQ list");
			return 1;
		}
		else {
			int i = 0;
			while (argv[1][i + offs] != 0x00 && argv[1][i + offs] != '"') {
				EQLIST[i] = argv[1][i + offs];
				i++;
			}
		}
	}
	char* CurrentResult;
	//copy the first argument into CurrentResult
	if (asprintf(&CurrentResult, "%s", argv[startIndex]) == -1) {
		fprintf(stderr, "ERROR: out of memory: could not even start");
		return 1;
	}
	//start at the second argument(i=1 then argv[i+1]) and assemble CurentResult and it.
	//NOTE: at the very beginning CurrentResult contains the FIRST argument since I copy it in there before starting
	//it is possible, the loop isn't executed AT ALL if only one argumet is given.
	for (int i = startIndex; i < argc - 1; i++) {
		char* IntermediateResult = reassemble(CurrentResult, argv[i + 1], EQLIST);
#ifdef DEBUG
		printf("[%s + %s -> %s]\n", CurrentResult, argv[i + 1], IntermediateResult);
#endif
		if (IntermediateResult != NULL) {
			if (CurrentResult != NULL) free(CurrentResult);
			CurrentResult = IntermediateResult;
			IntermediateResult = NULL;
		}
		else {
			fprintf(stderr, "ERROR: out of memory. Result is INCOMPLETE\n");
			printf("%s\n", CurrentResult);
			if (CurrentResult != NULL) free(CurrentResult);
			return 2;
		}
	}
	printf("%s\n", CurrentResult);
	if (CurrentResult != NULL) free(CurrentResult);
	return 0;
}
