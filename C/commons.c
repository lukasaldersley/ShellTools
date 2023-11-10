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

char ToLowerCase(char c)
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

char ToUpperCase(char c)
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
	bool matching = true;
	int idx = 0;
	while (matching && a[idx] != 0x00 && b[idx] != 0x00)
	{
		//fprintf(stderr, "a: %1$c <0x%1$x> b: %2$c <0x%2$x>\n", a[idx], b[idx]);
		if (a[idx] != b[idx])
		{
			//fprintf(stderr, "mismatch between %1$c <0x%1$x> and %2$c <0x%2$x>\n", a[idx], b[idx]);
			matching = false;
			break;
		}
		else if (b[idx] == 0x00)
		{
			//fprintf(stderr, "reached end of b -> full match complete\n");
			matching = true;
			break;
			// the two strings differ, but the difference is b ended whil a continues, therefore a starts with b
		}
		idx++;
	}
	return matching;
}

bool ContainsString(const char* str, const char* test)
{
	int sIdx = 0;
	while (str[sIdx] != 0x00)
	{
		uint8_t tIdx = 0;
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

void TerminateStrOn(char* str, const char* terminators) {
	int i = 0;
	while (str[i] != 0x00) {
		int t = 0;
		while (terminators[t] != 0x00) {
			if (str[i] == terminators[t]) {
				str[i] = 0x00;
				return;
			}
			t++;
		}
		i++;
	}
}

int LastIndexOf(const char* txt, char tst) {
	int idx = -1;
	int ridx = 0;
	while (txt[ridx] != 0x00) {
		if (txt[ridx] == tst) {
			idx = ridx;
		}
		ridx++;
	}
	return idx;
}

char* ExecuteProcess(const char* command) {
	int size = 1024;
	char* result = malloc(sizeof(char) * size);
	if (result == NULL) {
		return NULL;
	}
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
	}
	pclose(fp);
	return result;
}

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
			while (s[idx] != 0x00 && (c & 0b11000000) == 0b10000000) {
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

char* AbbreviatePathAuto(const char* path, int KeepAllIfShorterThan, int DesiredKeepElements) {
	//plausi-check the n umber of elements to keep and distribute them between front and back
	if (DesiredKeepElements < 1) { DesiredKeepElements = 1; }
	int KeepFront = DesiredKeepElements / 2;
	int KeepBack = DesiredKeepElements - KeepFront;
	return AbbreviatePath(path, KeepAllIfShorterThan, KeepFront, KeepBack);
}

char* AbbreviatePath(const char* path, int KeepAllIfShorterThan, int DesiredKeepElementsFront, int DesiredKeepElementsBack) {
	if (KeepAllIfShorterThan < 1) { KeepAllIfShorterThan = 1; }
	char* Workpath;
	if (asprintf(&Workpath, "%s", path) == -1)abortNomem();
	int len = strlen(Workpath);
	char* ret = NULL;
	if (len <= KeepAllIfShorterThan) {
		//text is so short, it won't be shortened anymore -> done
		if (asprintf(&ret, "%s", Workpath) == -1)abortNomem();
	}
	else {
		char* FromBack = Workpath + len - 1;
		char* FromFront = Workpath;
		int foundFront = 0;
		int foundBack = 0;
		int backLen = 0;
		int frontLen = 0;
		//if the string ends with / (or multiple of them), delete them, but keep at least one char in the entire string (eg the string is "/////", which in Unix just means /, so I too can shorten the string to /)
		while (*FromBack == '/' && FromBack > FromFront) {
			*FromBack = 0x00;
			FromBack--;
			len--;
		}
		//the string has potentially become short enough to be done already
		if (len <= KeepAllIfShorterThan) {
			if (asprintf(&ret, "%s", Workpath) == -1)abortNomem();
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
						;//this is a special case: I wan to count "/mnt/*/" as a single element, since just /mnt alone is kinda worthless -> skip over a single / if the conditions are right
					}
					else {
						foundFront++;
					}
				}
			}
		}
		if (FromBack - FromFront <= 3) {
			//the text would become longer by inserting the ..., so just keep the original text
			//this would happen with something like AbbreviatePath(/mnt/c/WS/CODE/BAT_VBS,20,3) where WS would be abbreiviated, but the abreviation ... is longer than the original name -> it doesn't make sense
			//printf("Front and bacl coincide\n");
			if (asprintf(&ret, "%s", Workpath) == -1)abortNomem();
		}
		else if (frontLen == 0) {
			//if the front segment doesn't exist, only print the back segment WITHOUT clobbering a seperator in front
			if (asprintf(&ret, "%s", FromBack + 1) == -1)abortNomem();
		}
		else {
			*(Workpath + frontLen) = 0x00;//truncate the work text to only contain the front segment
			if (asprintf(&ret, "%s/[...]/%s", Workpath, FromBack + 1) == -1)abortNomem();
		}
		//printf("%s & %s (%i + %i)\n", Workpath, FromBack + 1, frontLen, backLen);
	}
	free(Workpath);
	return ret;
}

void abortNomem() {
	printf("Out of memory\n");
	fprintf(stderr, "Out of memory\n");
	fflush(stdout);
	fflush(stderr);
	exit(1);
}
