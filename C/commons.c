#/*
echo "$0 is library file -> skip"
exit
*/

#include "commons.h"

void abortNomem() {
	abortMessage("Out of memory\n");
}

void abortMessage(const char* message) {
	printf("%s", message);
	fprintf(stderr, "%s", message);
	fflush(stdout);
	fflush(stderr);
	exit(1);
}

bool Compare(const char* a, const char* b)
{
	if (a == b) {
		return true;
	}
	if (a == NULL || b == NULL) {
		return false;
	}
	bool matching = true;
	int idx = 0;
	while (matching && (a[idx] != 0x00 || b[idx] != 0x00))
	{
		if (a[idx] != b[idx])
		{
			matching = false;
			break;
		}
		idx++;
	}
	return matching;
}

StringRelations CompareStrings(const char* a, const char* b) {
	StringRelations result = ALPHA_EQUAL;
	int idx = 0;
	while (a[idx] != 0x00 && b[idx] != 0x00) {
		char ca = ToLowerCase(a[idx]);
		char cb = ToLowerCase(b[idx]);
		if (ca != cb)
		{
			if (ca < cb) {
				result = ALPHA_BEFORE;
				break;
			}
			else {
				result = ALPHA_AFTER;
				break;
			}
		}
		idx++;
	}
	return result;
}

bool StartsWith(const char* a, const char* b)
{
	if (a == b) {
		return true;
	}
	if (a == NULL || b == NULL) {
		return false;
	}
	bool matching = true;
	int idx = 0;
	while (matching && a[idx] != 0x00 && b[idx] != 0x00)
	{
#ifdef DEBUG
		fprintf(stderr, "a: %1$c <0x%1$x> b: %2$c <0x%2$x>\n", a[idx], b[idx]);
#endif
		if (a[idx] != b[idx])
		{
#ifdef DEBUG
			fprintf(stderr, "mismatch between %1$c <0x%1$x> and %2$c <0x%2$x>\n", a[idx], b[idx]);
#endif
			matching = false;
			break;
		}
		idx++;
	}
	if (b[idx] != 0x00 && a[idx] == 0x00) {
		//a ended, but b hasn't -> a cannot contain b
#ifdef DEBUG
		fprintf(stderr, "a ended, but b continues with %1$c <0x%1$x>\n", b[idx]);
#endif
		return false;
	}
	return matching;
}

bool ContainsString(const char* str, const char* test)
{
	uint32_t sIdx = 0;
	while (str[sIdx] != 0x00)
	{
		uint32_t tIdx = 0;
		while (test[tIdx] != 0x00 && test[tIdx] == str[sIdx + tIdx])
		{
			//TODO optimize this to be a faster text search instead of adumb exhaustive searhc, if possible skip sIdx forward at the same time as tIdx
			tIdx++;
		}
		if (test[tIdx] == 0x00)
		{
			// if I reached the end of the test sting while the consition that test and str must match, test is contained in match
			return true;
		}
		sIdx++;
	}
	return false;
}

int16_t LastIndexOf(const char* txt, char tst) {
	if (txt == NULL) {
		return -2;
	}
	int16_t idx = -1;
	int16_t searchIndex = 0;
	while (txt[searchIndex] != 0x00) {
		if (txt[searchIndex] == tst) {
			idx = searchIndex;
		}
		searchIndex++;
	}
	if (tst == 0x00) {
		//if I searched for the last index of the null-byte, return what is essentially strlen
		return searchIndex;
	}
	return idx;
}

int16_t NextIndexOf(const char* txt, char tst, int startindex) {
	if (txt == NULL || startindex < 0) {
		//basically nonsense inputs (start before the first element or passed unallocated string) -> just quit with error
		return -2;
	}
	int16_t idx = -1;
	int16_t searchIndex = 0;
	while (searchIndex < startindex) {
		if (txt[searchIndex] == 0x00) {
			//sanity check; ensure end of string isn't before startindex
#ifdef DEBUG
			fprintf(stderr, "WARNING: StartIndex %i is out of bounds at %i for %s in NextIndexOf()\n", startindex, searchIndex, txt);
#endif
			return -2;
		}
		searchIndex++;
	}
	searchIndex = startindex;
	while (txt[searchIndex] != 0x00) {
		if (txt[searchIndex] == tst) {
			idx = searchIndex;
			break;
		}
		searchIndex++;
	}
	return idx;
}

/*This is basically strlen, but only counts VISIBLE characters
THIS MUST NOT BE USED TO DETERMINE REQUIRED BUFFER SIZES
A Usecase for this is computing how much space on screen is taken up by a string accounting for the fact control characters don't take up space
an example of this can be found in repotools.c
*/
int strlen_visible(const char* s) {
	int count = 0;
	int idx = 0;
	char c;
	while ((c = s[idx]) != 0x00) {
		//test for zsh prompt stuff
		if (c == '%') {
			//%F{...} -> set colour
			if (s[idx + 1] == 'F' && s[idx + 2] == '{') {
				while (s[idx] != 0x00 && s[idx] != '}') {
					idx++;
				}
				idx++;
				continue;
			}
			//%f -> clear colour
			else if (s[idx + 1] == 'f') {
				idx += 2;
				continue;
			}
			//%b -> clear bold
			else if (s[idx + 1] == 'b') {
				idx += 2;
				continue;
			}
			//%B -> set bold
			else if (s[idx + 1] == 'B') {
				idx += 2;
				continue;
			}
			//%% -> escaped %, an actual % sign
			else if (s[idx + 1 == '%']) {
				//NOTE: NO continue and ONLY +1 because I WANT to read that
				idx++;
			}
		}
		//ANSI CSI sequences https://en.wikipedia.org/wiki/ANSI_escape_code
		//don't count them at all
		if (c == '\e' && s[idx + 1] == '[') {
			idx += 2;//advance over \e[to test for the rest
			//walk until valid 'final byte (0x40-0x7e)' or NULL found
			while (s[idx] != 0x00 && !(s[idx] >= 0x40 && s[idx] <= 0x7E)) {
				idx++;
			}
		}
		//UTF-8 multibyte characters -> only count them once
		if ((c & 0b11000000) == 0b11000000) {
			//begins with 11... -> UTF8
			count++;
			while (s[idx] != 0x00 && (s[idx] & 0b11000000) == 0b10000000) {
				//in utf-8 the first byte is 11------ while all following bytes are 10------
				idx++;
			}
		}
		//all basic ascii, excluding the first 0x20 control chars and the DEL on 0x7f
		if (c >= 0x20 && c <= 0x7e) {
			count++;
		}
		idx++;
	}
	return count;
}

uint32_t TerminateStrOn(char* str, const char* terminators) {
	if (str == NULL || terminators == NULL) {
		return 0;
	}
	uint32_t i = 0;
	while (i < UINT32_MAX && str[i] != 0x00) {
		int8_t t = 0;
		while (t < UINT8_MAX && terminators[t] != 0x00) {
			if (str[i] == terminators[t]) {
				str[i] = 0x00;
				return i;
			}
			t++;
		}
		i++;
	}
	return i;
}

int cpyString(char* dest, const char* src, int maxCount) {
	for (int i = 0; i < maxCount; i++) {
		dest[i] = src[i];
		if (src[i] == 0x00) {
			return i;
		}
	}
	return maxCount;//shouldn't this be i?
}

inline char ToLowerCase(const char c)
{
	if (c >= 'A' && c <= 'Z')
	{
		return c + 0x20; // A=0x41, a=0x61
	}
	else
	{
		return c;
	}
}

inline char ToUpperCase(const char c)
{
	if (c >= 'a' && c <= 'z')
	{
		return c - 0x20;
	}
	else
	{
		return c;
	}
}

char* ExecuteProcess_alloc(const char* command) {
	int size = 1024;
	char* result = malloc(sizeof(char) * size);
	if (result == NULL) ABORT_NO_MEMORY;
	FILE* fp = popen(command, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "failed running process %s\n", command);
	}
	else {
		if (fgets(result, size - 1, fp) == NULL)
		{
			/*
			RETURN VALUE
				fgetc(), getc(), and getchar() return the character read as an unsigned char cast to an int or EOF on end of file or error.
				fgets() returns s on success, and NULL on error or when end of file occurs while no characters have been read.
			*/
			//show superprojet working tree returns 0 bytes if it's a toplevel thing -> just print back an empty string
			result[0] = 0x00;
		}
		pclose(fp);
	}
	return result;
}

int AbbreviatePathAuto(char** ret, const char* path, uint16_t KeepAllIfShorterThan, uint8_t DesiredKeepElements) {
	//plausi-check the number of elements to keep and distribute them between front and back, ensure there is at least one back element
	if (DesiredKeepElements < 1) {
		//if the number of KeepFront and KeepBack are both 0, I designed AbbreviatePath to act as sumb truncation or error out if the given length is less than 5 (for [...])
		//in my Shell I always want at least one element, therefore force at least one element
		DesiredKeepElements = 1;
	}
	uint8_t KeepFront = DesiredKeepElements / 2; //integer division, this means truncate decimals, if DesiredKeepElements is 1, that element will go to the back
	uint8_t KeepBack = DesiredKeepElements - KeepFront;
	return AbbreviatePath(ret, path, KeepAllIfShorterThan, KeepFront, KeepBack);
}

int AbbreviatePath(char** ret, const char* path, uint16_t KeepAllIfShorterThan, uint8_t DesiredKeepElementsFront, uint8_t DesiredKeepElementsBack) {
	if (ret == NULL) {
		//ret itself mustn't be 0, the char* ret points to actually should be, but ret itself MUST be valid
		return 1;
	}
	uint16_t len = (path == NULL ? 0 : strlen(path));
	char* Workpath;
	Workpath = malloc(sizeof(char) * len + 1);
	if (Workpath == NULL) ABORT_NO_MEMORY;
	Workpath[len] = 0x00;
	if (path == NULL) {
		//in all other paths workpath is just an internal variable that is free'd before the function exits since it's contents are asprintf'd into ret, here I just push the pointer over and never alocate a second memory region
		*ret = Workpath;
		return 1;
	}
	char lastChar = 0x00;
	uint16_t newLen = 0;
	for (int i = 0;i < len;i++) {
		//copy path into workpath, but skip any consecutive / (at any point in the string)
		if (!(path[i] == lastChar && lastChar == '/')) {
			Workpath[newLen] = path[i];
			newLen++;
		}
		lastChar = path[i];
		if (path[i] == 0x00) {
			break;
		}
	}
	Workpath[newLen] = 0x00;
	//if the text ends with / but that same /isnt't the first and only char, remove the trailing /
	if (newLen > 1 && Workpath[newLen - 1] == '/') {
		Workpath[newLen - 1] = 0x00;
		newLen--;
	}
	if (newLen <= KeepAllIfShorterThan) {
		//text is so short, it won't be shortened anymore -> done
		if (asprintf(ret, "%s", Workpath) == -1) ABORT_NO_MEMORY;
	}
	else if (DesiredKeepElementsBack == 0 && DesiredKeepElementsFront == 0) {
		//dumb truncation but from the back, NOTE: when calling via AbbreviatePathAuto this can never happen, since AbbreviatePathAuto ensures DesiredKeepElementsBack is at least 1
		(*ret) = malloc((sizeof(char) * KeepAllIfShorterThan) + 1);
		if ((*ret) == NULL) ABORT_NO_MEMORY;
		(*ret)[KeepAllIfShorterThan] = 0x00;
		if (KeepAllIfShorterThan < 5) {
			//requested dumb truncation, but the truncation indicator doesn't fit -> just print an error char
			for (int i = 0;i < KeepAllIfShorterThan;i++) {
				(*ret)[i] = '!';
			}
		}
		else {
			//the entire text doesn't fit in the specified size (checked earlier) and there is at least enought space for the truncation marker
			//print the truncation marker and as many chars from the end of the string as will fit
			(*ret)[0] = '[';
			(*ret)[1] = '.';
			(*ret)[2] = '.';
			(*ret)[3] = '.';
			(*ret)[4] = ']';
			for (int i = (newLen - (KeepAllIfShorterThan - 5)), j = 5;j < KeepAllIfShorterThan;i++, j++) {
				(*ret)[j] = Workpath[i];
			}
		}
	}
	else {
		//element-based smart truncation
		//if (asprintf(&Workpath, "%s", path) == -1) ABORT_NO_MEMORY;
		char* FromBack = Workpath + newLen - 1;
		char* FromFront = Workpath;
		uint16_t backLen = 0;
		uint16_t frontLen = 0;
		uint8_t foundFront = 0;
		uint8_t foundBack = 0;
		//walk backwards over the string until I identified DesiredKeepElementsBack elements (while NOT overruning the start of the string if there's fewer)
		while (foundBack<DesiredKeepElementsBack && FromBack>FromFront) {
			FromBack--;
			backLen++;
			if (*FromBack == '/' || *FromBack == '\\') {
				foundBack++;
			}
		}
		//walk forward over the string intil I identified DesiredKeepElementsFront elements (while NOT overrunning the end of the string or an element belonging to the back group)
		while (foundFront < DesiredKeepElementsFront && FromFront < FromBack) {
			FromFront++;
			frontLen++;
			if (*FromFront == '/' || *FromFront == '\\') {
				if (frontLen == 4 && *(FromFront + 2) == '/' && StartsWith(Workpath, "/mnt/")) {
					;//this is a special case: I want to count "/mnt/*/" as a single element, since just /mnt alone is kinda worthless -> skip over a single / if the conditions are right; this is mostly relevant for WSL where windows's Drive letters are assigned that way. C:\ becomes /mnt/c/
				}
				else {
					foundFront++;
				}
			}
		}
		if ((uint64_t)FromBack - (uint64_t)FromFront <= 6) {
			//the text would become longer by inserting the ..., so just keep the original text
			//this would happen with something like AbbreviatePath(/mnt/c/WS/CODE/BAT_VBS,20,3) where WS would be abbreiviated, but the abreviation ... is longer than the original name -> it doesn't make sense
			if (asprintf(ret, "%s", Workpath) == -1) ABORT_NO_MEMORY;
		}
		else if (frontLen == 0) {
			//if the front segment doesn't exist, only print the back segment WITHOUT clobbering a seperator in front
			if (asprintf(ret, "%s", FromBack + 1) == -1) ABORT_NO_MEMORY;
		}
		else {
			*(Workpath + frontLen) = 0x00;//truncate the work text to only contain the front segment
			if (asprintf(ret, "%s/[...]/%s", Workpath, FromBack + 1) == -1)ABORT_NO_MEMORY;
		}
		//printf("%s & %s (%i + %i)\n", Workpath, FromBack + 1, frontLen, backLen);
	}
	free(Workpath);
	return 0;
}

uint32_t determinePossibleCombinations(int* availableLength, int NumElements, ...) {
	//this function takes a poiner to an int containing the total available size and the number of variadic arguments to be expected.
	//the variadic elements are the size of individual blocks.
	//the purpose of this function is to figure out which blocks can fit into the total size in an optimal fashion.
	//an optimal fashion means: as many as possible, but the blocks are given in descending priority.
	//example: if there's a total size of 10 and the blocks 5,7,6,3,4,2,8,1,1,1,1,1,1,1,1 the solution would be to take 5+3+2 since 5 is the most important which means there's a size of 5 left that can be filled again.
	//7 doesn't fit, so we'll take the next best thing that will fit, in this case 3, which leaves 2, which in turn can be taken by the 2.
	//if the goal was just to have "as many as possible" the example should have picked all 1es, but since I need priorities, take the first that'll fit and find the next hightest priority that'll fit
	//(which will be further back in the list, otherwise it would already have been selected)
	//this function then returns a bitfield of which blocks were selected
	assert(NumElements > 0 && NumElements <= 32);
	uint32_t res = 0;
	va_list ELEMENTS;
	va_start(ELEMENTS, NumElements);//start variadic function param handling, NumElements is the Identifier of the LAST NON-VARIADIC parameter passed to this function
	for (int i = 0; i < NumElements; i++)
	{
		//va_arg returns the next of the variadic emlements, assuming it's type is compatible with the provided one (here int)
		//if it's not compatible, it's undefined behaviour
		int nextElem = va_arg(ELEMENTS, int);
		//if the next element fits, select it and reduce the available space
		if (*availableLength > nextElem) {
			res |= 1 << i;
			*availableLength -= nextElem;
		}
	}
	va_end(ELEMENTS);//a bit like malloc/free there has to be a va_end for each va_start

	return res;
}
