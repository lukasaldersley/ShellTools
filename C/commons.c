#/*
echo "$0 is library file -> skip"
exit
*/

#include "commons.h"

#include <assert.h> // for assert
#include <limits.h> // for __WORDSIZE
#include <stdarg.h> // for va_arg, va_end, va_list, va_start
#include <stdio.h> // for NULL, asprintf, fprintf, stderr, fflush, printf, fgets, pclose, popen, FILE, stdout
#include <string.h> // for strlen

void abortNomem() {
	abortMessage("Out of memory\n");
}

void abortMessage(const char* message) {
	fprintf(stdout, "%s\n", message);
	fflush(stdout);
	fprintf(stderr, "%s\n", message);
	fflush(stderr);
	exit(1);
}

bool Compare(const char* a, const char* b) {
	if (a == b) {
		//pointer comparison, also true if both are NULL
		return true;
	}
	if (a == NULL || b == NULL) {
		return false;
	}
	bool matching = true;
	int idx = 0;
	while (matching && (a[idx] != 0x00 || b[idx] != 0x00)) {
		if (a[idx] != b[idx]) {
			matching = false;
			break;
		}
		idx++;
	}
	return matching;
}

typedef enum {
	CompareModeLexicographical,
	CompareModeCaseInsensitive,
	CompareModeAsciiByteValues
} StringCompareMode;

static StringRelations CompareStringsInternal(const char* a, const char* b, StringCompareMode mode) {
	if (a == b) {
		//pointer comparison, also true if both are NULL
		return ALPHA_EQUAL;
	}
	if (a == NULL) {
		return ALPHA_BEFORE;
	}
	if (b == NULL) {
		return ALPHA_AFTER;
	}
	StringRelations result = ALPHA_EQUAL;
	int idx = 0;
	while (a[idx] != 0x00 && b[idx] != 0x00) {
		//in Lowercase A==a, in lexicographical a<A but b>A, in ascii go strict by byte numericals, ignoring any unicode (unicode is always after ascii here)
		uint32_t ca;
		uint32_t cb;
		switch (mode) {
			case CompareModeLexicographical: {
				//order: space<numbers<Letters (small before big)<Umlaute
				ca = ToLowerCase(a[idx]);
				cb = ToLowerCase(b[idx]);
#ifndef DISABLE_EXPECTED_WARNING_FOR_INTERNAL_TESTS
				fprintf(stderr, "CompareModeLexicographical is NOT implemented, falling back to CompareModeCaseInsensitive\n");
				//Note: the header for CompareStringsLexicographical is commented out since it's not really implemented
#endif
			} break;
			case CompareModeCaseInsensitive: {
				ca = ToLowerCase(a[idx]);
				cb = ToLowerCase(b[idx]);
			} break;
			case CompareModeAsciiByteValues: {
				ca = a[idx];
				cb = b[idx];
			} break;
			default: {
				fprintf(stderr, "INVLAID ENUM VALUE %i in %s:%i\n", mode, __FILE__, __LINE__);
				fflush(stderr);
				exit(1);
			}
		}
		if (ca != cb) {
			if (ca < cb) {
				result = ALPHA_BEFORE;
				break;
			} else {
				result = ALPHA_AFTER;
				break;
			}
		}
		idx++;
	}
	if (a[idx] == 0x00 && b[idx] != 0x00) {
		return ALPHA_BEFORE;
	}
	if (a[idx] != 0x00 && b[idx] == 0x00) {
		return ALPHA_AFTER;
	}
	return result;
}

StringRelations CompareStringsLexicographical(const char* a, const char* b) {
	return CompareStringsInternal(a, b, CompareModeLexicographical);
}

StringRelations CompareStringsCaseInsensitive(const char* a, const char* b) {
	return CompareStringsInternal(a, b, CompareModeCaseInsensitive);
}

StringRelations CompareStringsAsciiByteValues(const char* a, const char* b) {
	return CompareStringsInternal(a, b, CompareModeAsciiByteValues);
}

bool StartsWith(const char* a, const char* b) {
	if (a == b) {
		//pointer comparison, also true if both are NULL
		return true;
	}
	if (a == NULL || b == NULL) {
		return false;
	}
	bool matching = true;
	int idx = 0;
	while (matching && a[idx] != 0x00 && b[idx] != 0x00) {
#ifdef DEBUG_txt
		fprintf(stderr, "a: %1$c <0x%1$x> b: %2$c <0x%2$x>\n", a[idx], b[idx]);
#endif
		if (a[idx] != b[idx]) {
#ifdef DEBUG_txt
			fprintf(stderr, "mismatch between %1$c <0x%1$x> and %2$c <0x%2$x>\n", a[idx], b[idx]);
#endif
			matching = false;
			break;
		}
		idx++;
	}
	if (b[idx] != 0x00 && a[idx] == 0x00) {
		//a ended, but b hasn't -> a cannot contain b
#ifdef DEBUG_txt
		fprintf(stderr, "a ended, but b continues with %1$c <0x%1$x>\n", b[idx]);
#endif
		return false;
	}
	return matching;
}

bool ContainsString(const char* str, const char* test) {
	if (str == test) {
		//pointer comparison, also true if both are NULL
		return true;
	}
	if (str == NULL || test == NULL) {
		return false;
	}
	uint32_t sIdx = 0;
	while (str[sIdx] != 0x00) {
		uint32_t tIdx = 0;
		while (test[tIdx] != 0x00 && test[tIdx] == str[sIdx + tIdx]) {
			//TODO optimize this to be a faster text search instead of a dumb exhaustive search, if possible skip sIdx forward at the same time as tIdx
			tIdx++;
		}
		if (test[tIdx] == 0x00) {
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

#define max(A, B) (A > B ? A : B)
/*This is basically strlen, but only counts VISIBLE characters
THIS MUST NOT BE USED TO DETERMINE REQUIRED BUFFER SIZES
A Usecase for this is computing how much space on screen is taken up by a string accounting for the fact control characters don't take up space
an example of this can be found in shelltoolsmain.c
*/
bool strlen_visible_config(StrlenStruct* ret, const char* charstring, bool considerZshEscapeSequences) {
	if (charstring == NULL) {
		ret->height = 0;
		ret->len = 0;
		return false;
	}
	const uint8_t* s = (const uint8_t*)charstring;
	int widestKnownLine = 0;
	int numLineBreaks = 1;
	int count = 0;
	int idx = 0;
	uint8_t c;
	while ((c = s[idx]) != 0x00) {
		//ANSI CSI sequences https://en.wikipedia.org/wiki/ANSI_escape_code
		//don't count them at all
		if (c == '\e' && s[idx + 1] == '[') {

			idx += 2; //advance over \e[to test for the rest
			//walk until valid 'final byte (0x40-0x7e)' or NULL found
			while (s[idx] != 0x00 && !(s[idx] >= 0x40 && s[idx] <= 0x7E)) {
				idx++;
			}
			idx++;
			continue;
		}
		//UTF-8 multibyte characters -> only count them once
		if ((c & 0b11000000) == 0b11000000) {
			//begins with 11... -> UTF8
			count++;
			idx++;
			bool haveAtLeastOneFollowUp = false;
			while (s[idx] != 0x00 && (s[idx] & 0b11000000) == 0b10000000) {
				//in utf-8 the first byte is 11------ while all following bytes are 10------
				idx++;
				haveAtLeastOneFollowUp = true;
			}
			if (!haveAtLeastOneFollowUp) {
				fprintf(stderr, "[SHELLTOOLS: WARNING] a byte starting with 11... indicating start of an UTF-8 character has been encountered, but no follow-up bytes were present => unknown/unexpected byte 0x%x\n", c);
			}
			continue;
		}
		//test for zsh prompt stuff
		if (considerZshEscapeSequences && c == '%') {
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
			else if (s[idx + 1] == '%') {
				//NOTE: NO continue and ONLY +1 because I WANT to read that, but only once
				idx++;
			} else {
#ifndef DISABLE_EXPECTED_WARNING_FOR_INTERNAL_TESTS
				fprintf(stderr, "[SHELLTOOLS: WARNING]: Encountered '%%' with non-recognized following char '0x%x' in strlen_visible in ZSH-prompt mode -> counted as literal '%%'.\n", s[idx + 1]);
				fprintf(stderr, "                       NOTE: ZSH's interpretation may well be different. Please report to ShellTools Developer\n");
				fprintf(stderr, "                       Full string: \"%s\"\n", s);
#endif
			}
		}
		if (s[idx] == '\t') { //0x09
#ifndef DISABLE_EXPECTED_WARNING_FOR_INTERNAL_TESTS
			fprintf(stderr, "[SHELLTOOLS: NOTE]: strlen_visible: \\t is considered to always have a length of 4, this cannot be assumed to always be true\n");
#endif
			count += 4;
			idx++;
			continue;
		}
		if (s[idx] == '\n') { //0x0a
			widestKnownLine = max(widestKnownLine, count);
			numLineBreaks++;
			count = 0;
			//reset count, but keep track of what count was before reset, increment lineCount
			idx++;
			continue;
		}
		if (s[idx] == '\v' || s[idx] == '\f') { //0x0b or 0x0c
			//do not reset count, or increase count, just increase number of linebreaks and nothing else
			numLineBreaks++;
			idx++;
			continue;
		}
		if (s[idx] == '\r') { //0x0d
			//reset count, but keep track of what count was before reset, return highest count
			widestKnownLine = max(widestKnownLine, count);
			count = 0;
			idx++;
			continue;
		}
		if (s[idx] < 0x20 || s[idx] == 0x7f) {
			//zero-width basic ascii
			//DEL (0x7f) usually is just invisible -> don't decrease the count, just treat as invisible
			idx++;
			continue;
		}
		//all basic ascii, excluding the first 0x20 control chars and the DEL on 0x7f
		//at this point I intentionally do not use c, but rather look up the index, as the index could have advanced away from c
		if (s[idx] >= 0x20 && s[idx] <= 0x7e) {
			count++;
			idx++;
		} else {
			//I have NOT continue'd out of here but have found a non-basic ascii I cannot explain.
			//The expectation is to be in a UTF-8 context so there should not be any high characters.
			//of course the UTF-8 start marker 0b11...... is expected, but handled above.
			//there also are UTF-8 continuation bytes, but those are also handled above (provided they are at an expected position)
			//this consequently means I have encountered something odd or non-basic ascii/non-UTF-8
			fprintf(stderr, "[SHELLTOOLS: WARNING] unexpected byte 0x%x\n", s[idx]);
		}
	}
	ret->height = numLineBreaks;
	ret->len = max(count, widestKnownLine);
	return true;
}

int strlen_visible_width(const char* charstring) {
	StrlenStruct str;
	strlen_visible_config(&str, charstring, false);
	return str.len;
}

// This is part of the "public" API -> it's fine if cppcheck believes this isn't needed
// cppcheck-suppress unusedFunction
bool strlen_visible(StrlenStruct* ret, const char* charstring) {
	return strlen_visible_config(ret, charstring, false);
}

/**
 * This function acts to replace any char in terminators with 0x00 to terminate the string.
 * Additionally the return value is the new length of the string including terminating 0x00
 */
uint32_t TerminateStrOn(char* str, const char* terminators) {
	if (str == NULL) {
		return 0;
	}
	uint32_t i = 0;
	while (i < UINT32_MAX && str[i] != 0x00) {
		uint8_t t = 0;
		while (t < UINT8_MAX && terminators != NULL && terminators[t] != 0x00) {
			if (str[i] == terminators[t]) {
				str[i] = 0x00;
				return i + 1;
			}
			t++;
		}
		i++;
	}
	return i;
}

/**
 * This function is an extension of TerminateStrOn(), as it does the exact same thing, but then trims configurable chars off the end
 */
uint32_t TerminateThenTrimStrOn(char* str, const char* terminators, const char* trimChars) {
	uint32_t restLen = TerminateStrOn(str, terminators);
	if (restLen == 0) {
		return restLen; //string already empty, I don't need to process further
	}
	uint32_t i = 0;
	while (str[i] != 0x00) {
		//this probably warrants explanation:
		//the outer while loop checking front-to-back if the end of string has been reached is just to prevent infinite loops caused by overflows/weird int casting I'd need to prevent said overflows if I were using a for loop
		//the actual check for trimability still is back-to-front nad uses the same index i, but subtracts it from the known text length, thereby reversing the traversal direction.
		//if a trimable char is found it's overwritten with 0x00 (this implicitly also moves the upper bound for loop executions forward, which isn't that relevant but 'accidentally' adds a tiny bit of efficiency if there's a lot to trim off)
		uint8_t t = 0;
		bool foundTrimable = false;
		while (t < UINT8_MAX && trimChars[t] != 0x00) {
			if (str[(restLen - 1) - i] == trimChars[t]) {
				str[(restLen - 1) - i] = 0x00;
				foundTrimable = true;
			}
			t++;
		}
		if (!foundTrimable) {
			break; //reached non-trim char -> stop
		}
		i++;
	}
	return (restLen - 1) - i;
}

int CopyStringNumChar(char* dest, const char* src, int maxCount) {
	return CopyStringNumCharConfig(dest, src, maxCount, false);
}

int CopyStringNumCharConfig(char* dest, const char* src, int maxCount, bool unEscapeSpecials) {
	if (src == NULL || dest == NULL) {
		return 0;
	}
	int i = 0;
	int j = 0;
	for (; i < maxCount; i++, j++) {
		if (unEscapeSpecials && src[j] == '\\') {
			int k = j + 1;
			switch (src[k]) {
				case 'e':
					dest[i] = '\e';
					j++;
					break;
				case 'n':
					dest[i] = '\n';
					j++;
					break;
				case 't':
					dest[i] = '\t';
					j++;
					break;
				case 'r':
					dest[i] = '\r';
					j++;
					break;

				default:
					//no known escape sequence -> copy as literal
					dest[i] = src[j];
					break;
			}
		} else {
			dest[i] = src[j];
		}
		if (src[i] == 0x00) {
			return i;
		}
	}
	return i;
}

//TODO implement (limited) Unicode support, by enabling the input of char arrays
inline char ToLowerCase(const char c) {
	if (c >= 'A' && c <= 'Z') {
		return c + 0x20; // A=0x41, a=0x61
	} else {
		return c;
	}
}

inline char ToUpperCase(const char c) {
	if (c >= 'a' && c <= 'z') {
		return c - 0x20;
	} else {
		return c;
	}
}

char* ExecuteProcess_alloc(const char* command) {
	int size = 1024;
	char* result = malloc(sizeof(char) * size);
	if (result == NULL) ABORT_NO_MEMORY;
	FILE* fp = popen(command, "r");
	if (fp == NULL) {
		fprintf(stderr, "failed running process %s\n", command);
	} else {
		if (fgets(result, size - 1, fp) == NULL) {
			/* from $> man fgets:
			RETURN VALUE
				fgetc(), getc(), and getchar() return the character read as an unsigned char cast to an int or EOF on end of file or error.
				fgets() returns s on success, and NULL on error or when end of file occurs while no characters have been read.
			*/
			//it is possible for a command to return 0 bytes in stdout, if so, just return empty string in that case
			result[0] = 0x00;
		}
		pclose(fp);
	}
	return result;
}

int AbbreviatePathAuto(char** ret, const char* path, uint16_t KeepAllIfShorterThan, uint8_t DesiredKeepElements) {
	//plausi-check the number of elements to keep and distribute them between front and back, ensure there is at least one back element
	if (DesiredKeepElements < 1) {
		//if the number of KeepFront and KeepBack are both 0, I designed AbbreviatePath to act as dumb truncation or error out if the given length is less than 5 (for [...])
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
		//in all other paths workpath is just an internal variable that is free'd before the function exits since it's contents are asprintf'd into ret, here I just push the pointer over and never allocate a second memory region
		*ret = Workpath;
		return 1;
	}
	char lastChar = 0x00;
	uint16_t newLen = 0;
	for (int i = 0; i < len; i++) {
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
	//if the text ends with / but that same /isn't the first and only char, remove the trailing /
	if (newLen > 1 && Workpath[newLen - 1] == '/') {
		Workpath[newLen - 1] = 0x00;
		newLen--;
	}
	if (newLen <= KeepAllIfShorterThan) {
		//text is so short, it won't be shortened anymore -> done
		if (asprintf(ret, "%s", Workpath) == -1) ABORT_NO_MEMORY;
	} else if (DesiredKeepElementsBack == 0 && DesiredKeepElementsFront == 0) {
		//dumb truncation but from the back, NOTE: when calling via AbbreviatePathAuto this can never happen, since AbbreviatePathAuto ensures DesiredKeepElementsBack is at least 1
		(*ret) = malloc((sizeof(char) * KeepAllIfShorterThan) + 1);
		if ((*ret) == NULL) ABORT_NO_MEMORY;
		(*ret)[KeepAllIfShorterThan] = 0x00;
		if (KeepAllIfShorterThan < 5) {
			//requested dumb truncation, but the truncation indicator doesn't fit -> just print an error char
			for (int i = 0; i < KeepAllIfShorterThan; i++) {
				(*ret)[i] = '!';
			}
		} else {
			//the entire text doesn't fit in the specified size (checked earlier) and there is at least enough space for the truncation marker
			//print the truncation marker and as many chars from the end of the string as will fit
			(*ret)[0] = '[';
			(*ret)[1] = '.';
			(*ret)[2] = '.';
			(*ret)[3] = '.';
			(*ret)[4] = ']';
			for (int i = (newLen - (KeepAllIfShorterThan - 5)), j = 5; j < KeepAllIfShorterThan; i++, j++) {
				(*ret)[j] = Workpath[i];
			}
		}
	} else {
		//element-based smart truncation
		//if (asprintf(&Workpath, "%s", path) == -1) ABORT_NO_MEMORY;
		char* FromBack = Workpath + newLen - 1;
		char* FromFront = Workpath;
		uint16_t backLen = 0;
		uint16_t frontLen = 0;
		uint8_t foundFront = 0;
		uint8_t foundBack = 0;
		//walk backwards over the string until I identified DesiredKeepElementsBack elements (while NOT overruning the start of the string if there's fewer)
		while (foundBack < DesiredKeepElementsBack && FromBack > FromFront) {
			FromBack--;
			backLen++;
			if (*FromBack == '/' || *FromBack == '\\') {
				foundBack++;
			}
		}
		//walk forward over the string until I identified DesiredKeepElementsFront elements (while NOT overrunning the end of the string or an element belonging to the back group)
		while (foundFront < DesiredKeepElementsFront && FromFront < FromBack) {
			FromFront++;
			frontLen++;
			if (*FromFront == '/' || *FromFront == '\\') {
				if (frontLen == 4 && *(FromFront + 2) == '/' && StartsWith(Workpath, "/mnt/")) {
					; //this is a special case: I want to count "/mnt/*/" as a single element, since just /mnt alone is kinda worthless -> skip over a single / if the conditions are right; this is mostly relevant for WSL where windows's Drive letters are assigned that way. C:\ becomes /mnt/c/
				} else {
					foundFront++;
				}
			}
		}
#if __WORDSIZE == 64
		if ((uint64_t)FromBack - (uint64_t)FromFront <= 6) {
#else
		if ((uint32_t)FromBack - (uint32_t)FromFront <= 6) {
#endif
			//the text would become longer by inserting the ..., so just keep the original text
			//this would happen with something like AbbreviatePath(/mnt/c/WS/CODE/BAT_VBS,20,3) where WS would be abbreviated, but the abbreviation ... is longer than the original name -> it doesn't make sense
			if (asprintf(ret, "%s", Workpath) == -1) ABORT_NO_MEMORY;
		}
#ifdef PATH_ABBREV_TREAT_SINGLE_ELEMENT_AS_RELATIVE_PATH
		//this is how this had been implemented in the past.
		//if the settings were to only keep a single element at the back, none at the front
		//if then the last directory in $(pwd) was longer than the truncation threshold, this would print that last directory
		//and ONLY that directory, without the omission marker, thereby behaving as if dieplaying a relative path.
		//that behaviour is not what I had intended, but I'll leave this in as a compiletime option
		else if (frontLen == 0) {
#else
		else if (frontLen == 0 && DesiredKeepElementsFront > 0) {
#endif
			//if the front segment doesn't exist, only print the back segment WITHOUT clobbering a separator in front
			if (asprintf(ret, "%s", FromBack + 1) == -1) ABORT_NO_MEMORY;
		} else {
			//truncate the work text to only contain the front segment
			if (*(Workpath + frontLen) == '/' || *(Workpath + frontLen) == '\\') {
				*(Workpath + frontLen + 1) = 0x00; //if the last character in the front is / or \, keep it as spacing from the [...] block.
				//This also doubles as an absolute/relative path handling, if the front section is nothing, this adds / to the front of the string IFF the input had it as well (ie was an absolute path)
			} else {
				*(Workpath + frontLen) = 0x00;
			}
			//the ternary expression is there to enable the / immediately after [...] IFF anything more follows after that by utilizing the / from the input
			if (asprintf(ret, "%s[...]%s", Workpath, FromBack + (backLen > 0 ? 0 : 1)) == -1) ABORT_NO_MEMORY;
		}
		//printf("%s & %s (%i + %i)\n", Workpath, FromBack + 1, frontLen, backLen);
	}
	free(Workpath);
	return 0;
}
/**
 * this function takes a pointer to an int containing the total available size and the number of variadic arguments to be expected.
 * NumElements (and therefore the number of variadic arguments to be considered) must be less than or equal to 32.
 * the variadic elements are the size of individual blocks.
 * the purpose of this function is to figure out which blocks can fit into the total size in an optimal fashion.
 * an optimal fashion means: as many as possible, but the blocks are given in descending priority.
 * example: if there's a total size of 10 and the blocks 5,7,6,3,4,2,8,1,1,1,1,1,1,1,1 the solution would be to take 5+3+2 since 5 is the most important which means there's a size of 5 left that can be filled again.
 * 7 doesn't fit, so we'll take the next best thing that will fit, in this case 3, which leaves 2, which in turn can be taken by the 2.
 * if the goal was just to have "as many as possible" the example should have picked all 1es, but since I need priorities, take the first that'll fit and find the next hightest priority that'll fit
 * (which will be further back in the list, otherwise it would already have been selected)
 * this function then returns a bitfield of which blocks were selected
 */
uint32_t determinePossibleCombinations(int* availableLength, int NumElements, ...) {
	if (NumElements == 0) {
		return 0;
	}
	assert(NumElements <= 32);
	uint32_t res = 0;
	va_list ELEMENTS;
	va_start(ELEMENTS, NumElements); //start variadic function param handling, NumElements is the Identifier of the LAST NON-VARIADIC parameter passed to this function
	for (int i = 0; i < NumElements; i++) {
		//va_arg returns the next of the variadic elements, assuming it's type is compatible with the provided one (here int)
		//if it's not compatible, it's undefined behaviour
		int nextElem = va_arg(ELEMENTS, int);
		//if the next element fits, select it and reduce the available space
		if (*availableLength >= nextElem) {
			res |= 1 << i;
			*availableLength -= nextElem;
		}
	}
	va_end(ELEMENTS); //a bit like malloc/free there has to be a va_end for each va_start

	return res;
}

static void CopyErrorByteToBuffer(char UTF8DestBuf[], const char* CodePoint) {
	UTF8DestBuf[0] = 0xEF;
	UTF8DestBuf[1] = 0xBF;
	UTF8DestBuf[2] = 0xBD;
	UTF8DestBuf[3] = 0x00;
	printf("ERROR: invalid Unicode codepoint >%s<\n", CodePoint);
}

static bool ParseUnicodeCPToUTF8String(const char* CodePoint, char UTF8DestBuf[]) {
	if (CodePoint[0] == 'U' && CodePoint[1] == '+') {
		//actual unicode decode
		char* RestChar;

		uint32_t integercp = strtoul(CodePoint + 2, &RestChar, 16);
		if (RestChar[0] != 0x00) {
			//if strtol has anything in char **_Nullable restrict endptr (here assigned to Restchar), there was something in the input (char **_Nullable restrict endptr) after the numbers -> failure
			CopyErrorByteToBuffer(UTF8DestBuf, CodePoint);
			return false;
		}
		if (integercp <= 0x7F) {
			// basic 7-bit Ascii, but encoded as unicode for some reason
			UTF8DestBuf[0] = (char)integercp;
			UTF8DestBuf[1] = 0;
			return true;
		} else if (integercp <= 0x07FF) {
			// above or equal to 0x80 but below or equal to 0x7FF -> Will fit in two-byte UTF-8
			//the second byte can contain 6 bits of 'payload', therfore the number is right shifted by 6
			//integer promotion means the right shift may shift-in logical 1 instad of 0 to preserve the sign bit
			//to prevent/reverse that I mask the three upper bits (the ones denoting a multibyte UTF-8 of two bytes) as zero, then force the bits to their correct positions
			//then I do something similar to the restbits (6 each in every continuation byte)
			//UTF8DestBuf[0] = (char)(((integercp >> 6) & 0x1F) | 0xC0);
			//UTF8DestBuf[1] = (char)(((integercp >> 0) & 0x3F) | 0x80);
			UTF8DestBuf[0] = (char)(((integercp >> 6) & 0b00011111) | 0b11000000);
			UTF8DestBuf[1] = (char)((integercp & 0b00111111) | 0b10000000);
			UTF8DestBuf[2] = 0;
			return true;
		} else if (integercp <= 0xFFFF) {
			// above or equal to 0x800 but below or equal to 0xFFFF -> Will fit in three-byte UTF-8
			//this works analogous to the block 0x80 to 0x7ff
			//UTF8DestBuf[0] = (char)(((integercp >> 12) & 0x0F) | 0xE0);
			//UTF8DestBuf[1] = (char)(((integercp >> 6) & 0x3F) | 0x80);
			//UTF8DestBuf[2] = (char)(((integercp >> 0) & 0x3F) | 0x80);
			UTF8DestBuf[0] = (char)(((integercp >> 12) & 0b00001111) | 0b11100000);
			UTF8DestBuf[1] = (char)(((integercp >> 6) & 0b00111111) | 0b10000000);
			UTF8DestBuf[2] = (char)((integercp & 0b00111111) | 0b10000000);
			UTF8DestBuf[3] = 0;
			return true;
		} else if (integercp <= 0x10FFFF) {
			// above or equal to 0x10000 but below or equal to 10FFFF -> Will fit in four byte UTF-8
			//this works analogous to the block 0x80 to 0x7ff
			UTF8DestBuf[0] = (char)(((integercp >> 18) & 0b00000111) | 0b11110000);
			UTF8DestBuf[1] = (char)(((integercp >> 12) & 0b00111111) | 0b10000000);
			UTF8DestBuf[2] = (char)(((integercp >> 6) & 0b00111111) | 0b10000000);
			UTF8DestBuf[3] = (char)((integercp & 0b00111111) | 0b10000000);
			UTF8DestBuf[4] = 0;
			return true;
		} else {
			// above or equal to 0x110000 -> guaranteed invalid -> error
			CopyErrorByteToBuffer(UTF8DestBuf, CodePoint);
			return false;
		}
	} else {
		//not supported format (doesn't start  with U+)

		CopyErrorByteToBuffer(UTF8DestBuf, CodePoint);
		return false;
	}
}

bool ParseCharOrCodePoint(const char* Input, char DestBuf[]) {
	if (Input[0] == '\'' && Input[2] == '\'' && Input[1] <= 0x7F) {
		//basic ASCII (between 0x00 and 0x7F inclusive), enclosed by single quotes can simply be copied over, everything else I need to attempt Unicode-codepoint decoding
		DestBuf[0] = Input[1];
		DestBuf[1] = 0x00;
		return true;
	} else {
		return ParseUnicodeCPToUTF8String(Input, DestBuf);
	}
}
