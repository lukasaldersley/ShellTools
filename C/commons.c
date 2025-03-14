#/*
echo "$0 is library file -> skip"
exit
*/

#include "commons.h"

bool Compare(const char* a, const char* b)
{
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

uint32_t TerminateStrOn(char* str, const char* terminators) {
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

char* ExecuteProcess(const char* command) {
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

char* AbbreviatePathAuto(const char* path, uint16_t KeepAllIfShorterThan, uint8_t DesiredKeepElements) {
	//plausi-check the number of elements to keep and distribute them between front and back
	if (DesiredKeepElements < 1) { DesiredKeepElements = 1; }
	uint8_t KeepFront = DesiredKeepElements / 2;
	uint8_t KeepBack = DesiredKeepElements - KeepFront;
	return AbbreviatePath(path, KeepAllIfShorterThan, KeepFront, KeepBack);
}

char* AbbreviatePath(const char* path, uint16_t KeepAllIfShorterThan, uint8_t DesiredKeepElementsFront, uint8_t DesiredKeepElementsBack) {
	if (KeepAllIfShorterThan < 1) { KeepAllIfShorterThan = 1; }
	char* Workpath;
	if (asprintf(&Workpath, "%s", path) == -1) ABORT_NO_MEMORY;
	uint16_t len = strlen(Workpath);
	char* ret = NULL;
	if (len <= KeepAllIfShorterThan) {
		//text is so short, it won't be shortened anymore -> done
		if (asprintf(&ret, "%s", Workpath) == -1) ABORT_NO_MEMORY;
	}
	else {
		char* FromBack = Workpath + len - 1;
		char* FromFront = Workpath;
		// cppcheck-suppress variableScope ; Reson: readability
		uint16_t backLen = 0, frontLen = 0;
		// cppcheck-suppress variableScope ; Reson: readability
		uint8_t foundFront = 0, foundBack = 0;
		//if the string ends with / (or multiple of them), delete them, but keep at least one char in the entire string (eg the string is "/////", which in Unix just means /, so I too can shorten the string to /)
		while (*FromBack == '/' && FromBack > FromFront) {
			*FromBack = 0x00;
			FromBack--;
			len--;
		}
		//the string has potentially become short enough to be done already
		if (len <= KeepAllIfShorterThan) {
			if (asprintf(&ret, "%s", Workpath) == -1) ABORT_NO_MEMORY;
		}
		else {
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
		}
		if ((uint64_t)FromBack - (uint64_t)FromFront <= 6) {
			//the text would become longer by inserting the ..., so just keep the original text
			//this would happen with something like AbbreviatePath(/mnt/c/WS/CODE/BAT_VBS,20,3) where WS would be abbreiviated, but the abreviation ... is longer than the original name -> it doesn't make sense
			if (asprintf(&ret, "%s", Workpath) == -1) ABORT_NO_MEMORY;
		}
		else if (frontLen == 0) {
			//if the front segment doesn't exist, only print the back segment WITHOUT clobbering a seperator in front
			if (asprintf(&ret, "%s", FromBack + 1) == -1) ABORT_NO_MEMORY;
		}
		else {
			*(Workpath + frontLen) = 0x00;//truncate the work text to only contain the front segment
			if (asprintf(&ret, "%s/[...]/%s", Workpath, FromBack + 1) == -1)ABORT_NO_MEMORY;
		}
		//printf("%s & %s (%i + %i)\n", Workpath, FromBack + 1, frontLen, backLen);
	}
	free(Workpath);
	return ret;
}

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
