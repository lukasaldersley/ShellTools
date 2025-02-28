#/*
TargetDir="$ST_CFG"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename "$0" .c).elf"
printf "compiling %s into " "$0" "$TargetDir/$TargetName"
#shellcheck disable=SC2086
gcc -O3 -std=c2x -Wall "$(realpath "$(dirname "$0")")"/commons.c "$0" -o "$TargetDir/$TargetName" $1
#the fact I DIDN'T add quotations to the $1 above means I WANT word splitting to happen.
#I WANT to be able to do things like ./repotools.c -DPROFILING to add the compiler flag profiling but ALSO stuff like ./repotools "-DDEBUG -DPROFILING" to add both profiling and debug
printf " -> \e[32mDONE\e[0m(%s)\n" $?
if [ "$(eval "$TargetDir/$TargetName --equal=\" v\" AbBxB B Bdef efghiC v3.17 \" 3.17.0\" FX")" = "AbBxBdefghiCv3.17.0FX" ] && \
	[ "$(eval "$TargetDir/$TargetName AbBxB B Bdef efghiC v3.17 \" 3.17.0\" FX")" = "AbBxBdefghiCv3.17 3.17.0FX" ] && \
	[ "$(eval "$TargetDir/$TargetName aaaa aaa aaAaa aab aa aaa aac")" = "aaaaAaabaaac" ] && \
	[ "$(eval "$TargetDir/$TargetName -i -e \" v\" aaaa aaa aaAaa aab aa aaa aac")" = "aaaaabaaac" ] && \
	[ "$(eval "$TargetDir/$TargetName -i -e \" v\" Kubuntu \"Ubuntu 24.10\" \" 24.10 (Oracular Oriole)\"")" = "Kubuntu 24.10 (Oracular Oriole)" ] && \
	[ "$(eval "$TargetDir/$TargetName --ignore-case aaaa aaa aaAaa aab aa aaa aac")" = "aaaaabaaac" ] ;
then
	printf "Tests for %s completed \e[32mOK\e[0m\n" "$TargetDir/$TargetName"
	exit 0
else
	if [ $# -ne 0 ]; then
		printf "Tests invalid due to build parameters, built-in tests only work without any parameters\n"
		exit 0
	else
		printf "Tests for %s completed \e[31mFAIL\e[0m\n" "$TargetDir/$TargetName"
		exit 1
	fi
fi
*/

//add option to have chars be treated as equal such as --equal=" v" therfore 0x20 and v are to be the same (to allow matching up v3.17 with " 3.17.0")
#include "commons.h"
#include <string.h>
#include <getopt.h>

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

bool checkEquality(char a, char b, bool caseIndependant, char** eqlists, int numEQList) {
	if (a == b || (caseIndependant && ToUpperCase(a) == ToUpperCase(b))) {
		return true;
	}
	for (int i = 0;i < numEQList;i++) {
		char* clist = *(eqlists + i);
		if (in_list(a, clist) && in_list(b, clist)) {
			return true;
		}
	}
	return false;
}

int assemble(char* targetBuffer, int targetBufferLength, int alreadyFixedChars, char* newSegmentBuffer, int newSegmentBufferLength, char** equalityLists, int numEqualityLists, bool caseIndependant) {
	//the new segment is added at the end, so if I were to add only a single char I don't need to check anything but the last char of the already established text, but I mustn't start before the start of the buffer
	int startOffset = alreadyFixedChars - newSegmentBufferLength;
	if (startOffset < 0) {
		startOffset = 0;
	}

	//since the assumption that the new string will be entirely absorbed in what's already present will be inaccurate most of the time I need to check if there actually is any overlap
	//realistically this could be a while(true) construct, since the return should happen inside this loop, it only isn't a while(true) to avoid the possibility of infinite loops
	while (startOffset <= alreadyFixedChars) {
		int testIndexOffset = 0;
		while (
			startOffset + testIndexOffset <= alreadyFixedChars && /**/
			startOffset + testIndexOffset < targetBufferLength &&
			testIndexOffset < newSegmentBufferLength &&
			checkEquality(*(targetBuffer + startOffset + testIndexOffset), *(newSegmentBuffer + testIndexOffset), caseIndependant, equalityLists, numEqualityLists))
		{
			//while I havn't overrun any array- or sense-boundaries, advance the test index as long as the two strings still match
			testIndexOffset++;
		}
		//the new string is contained in the already present text, just not at the end, advance the base offset and try again
		if (startOffset + testIndexOffset != alreadyFixedChars) {
			startOffset++;
			continue;
		}
		//start at the first character from the new string that is NOT ALREADY in the base string and copy the rest over
		//this also defines the behaviour of equality classes, since whatever was in the earlier string is what ends up in the result
		//for example if it's case-insensitive (eg each letter is in an equality class with both it's forms A and a) and the words were "OperatingSystem" and "systemd" the result would be "OperatingSystemd" not "Operatingsystemd"
		for (; testIndexOffset < newSegmentBufferLength; testIndexOffset++) {
			*(targetBuffer + startOffset + testIndexOffset) = *(newSegmentBuffer + testIndexOffset);
		}
		//return the new total length of properly assembled text
		return startOffset + newSegmentBufferLength;
	}
	//this should never be reached, and if it is my algorithm is crap and dangerous
	return -1;
}

int main(int argc, char** argv) {

	int numeqlists = 0;
	//allocate space for as many pointers to equality lists as there are arguments (for the worst case of every single argument is an equality list definition [that doesn't make sense, but it is the upper boundary])
	char** eqlists = (char**)malloc(sizeof(char*) * argc);
	if (eqlists == NULL)abortNomem();
	//null-initialize array to cause obvious errors if something goes wrong
	for (int i = 0;i < argc;i++) {
		*(eqlists + i) = NULL;
	}

	bool ignoreCase = false;

	//the tempCounter isn't used other than as a safeguard agains infinite loops
	int tempCounter = 0;
	while (tempCounter < argc) {
		tempCounter++;

		int getopt_currentChar;
		int option_index = 0;

		static struct option long_options[] = {
			{"equal", required_argument, 0, 'e' },
			{"ignore-case", no_argument, 0, 'i'},
			{0, 0, 0, 0 }
		};

		getopt_currentChar = getopt_long(argc, argv, "ie:", long_options, &option_index);
		if (getopt_currentChar == -1) {
			break;
		}

		switch (getopt_currentChar) {
		case 0:
			{
				printf("long option %s", long_options[option_index].name);
				if (optarg) {
					printf(" with arg %s", optarg);
				}
				printf("\n");
				break;
			}
		case 'i':
			{
				ignoreCase = true;
				break;
			}
		case 'e':
			{
				if (asprintf((eqlists + numeqlists), "%s", optarg) == -1)abortNomem();
				numeqlists++;
				break;
			}
		case '?':
			{
				printf("option %c: >%s< is not a recognized option\n", getopt_currentChar, optarg);
				break;
			}
		default:
			{
				printf("?? getopt returned character code 0%o (char: %c) with option %s ??\n", getopt_currentChar, getopt_currentChar, optarg);
				exit(1);
			}
		}
	}

#ifdef DEBUG
	for (int i = 0;i < argc;i++) {
		printf("%soption-arg %i:\t>%s<\n", (i >= optind ? "non-" : "\t"), i, argv[i]);
	}
#endif

	int maxSize = 1;
	for (int i = optind;i < argc;i++) {
		int tl = strlen(*(argv + i));
		maxSize += tl;
#ifdef DEBUG
		printf("%s is %i chars -> running total is %i\n\n", *(argv + i), tl, maxSize);
#endif
	}
	char* base = (char*)malloc(sizeof(char) * maxSize);
	for (int i = 0;i < maxSize;i++) {
		*(base + i) = 0x00;
	}
	int knownlen = 0;

#ifdef DEBUG
	for (int i = 0;i < numeqlists;i++) {
		char* clist = *(eqlists + i);
		printf("%i. equality list: >%s<\n", i, clist);
	}
#endif

	for (int i = optind; i < argc;i++) {
#ifdef DEBUG
		printf("{[%s] + [%s] -> ", base, *(argv + i));
#endif
		knownlen = assemble(base, maxSize, knownlen, *(argv + i), strlen(*(argv + i)), eqlists, numeqlists, ignoreCase);
		if (knownlen < 0) {
			fprintf(stderr, "something went HORRIBLY wrong and my algorithm is FUNDAMENTALLY FALSE\n");
			return 1;
		}
#ifdef DEBUG
		printf("[%s]}\n", base);
#endif
	}
	printf("%s\n", base);

	return 0;
}
