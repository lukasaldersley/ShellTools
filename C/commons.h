#ifndef __BVBS_ZSH_COMMONS__
#define __BVBS_ZSH_COMMONS__

#define _GNU_SOURCE
#include <stdbool.h> // for bool, false, true
#include <stdint.h> // for uint8_t, int16_t, uint16_t, uint32_t
#include <stdlib.h> // for exit

#define COLOUR_GREYOUT "\e[38;5;240m"
#define COLOUR_CLEAR   "\e[0m"
#define MODIFIER_BOLD  "\e[1m"

#define DEFAULT_TERMINATORS "\r\n\a"

#define ABORT_NO_MEMORY {abortNomem();exit(1);}

typedef struct {
	int height;
	int len;
} StrlenStruct;

typedef enum {
	ALPHA_BEFORE,
	ALPHA_EQUAL,
	ALPHA_AFTER,
} StringRelations;

//System functions -> not covered by 0_st_tests.c (due to system dependencies/side effects)
void abortNomem();
void abortMessage(const char* message);
char* ExecuteProcess_alloc(const char* command);

//Immutable functions covered in 0_st_tests.c using TestWithExpectaion
bool Compare(const char* a, const char* b);
//StringRelations CompareStringsLexicographical(const char* a, const char* b);//not yet implemented -> just a placeholder
StringRelations CompareStringsCaseInsensitive(const char* a, const char* b);
StringRelations CompareStringsAsciiByteValues(const char* a, const char* b);
bool StartsWith(const char* a, const char* b);
bool ContainsString(const char* str, const char* test);
int16_t LastIndexOf(const char* txt, char tst);
int16_t NextIndexOf(const char* txt, char tst, int startindex);
char ToLowerCase(const char c);
char ToUpperCase(const char c);
bool strlen_visible_config(StrlenStruct* ret, const char* charstring, bool considerZshEscapeSequences);

//Immutable functions, implicitly covered in 0_st_tests.c via strlen_visible_config
bool strlen_visible(StrlenStruct* ret, const char* charstring);
int strlen_visible_width(const char* charstring);

//Mutable functions, not yet covered in 0_st_tests with custom tailored framework
uint32_t TerminateStrOn(char* str, const char* terminators);
uint32_t TerminateThenTrimStrOn(char* str, const char* terminators, const char* trimChars);
uint32_t determinePossibleCombinations(int* availableLength, int NumElements, ...);
bool ParseCharOrCodePoint(const char* Input, char DestBuf[]);
int CopyStringNumChar(char* dest, const char* src, int maxCount);
int CopyStringNumCharConfig(char* dest, const char* src, int maxCount, bool unEscapeSpecials);
//Mutable functions, covered in 0_st_tests with custom tailored framework
int AbbreviatePath(char** ret, const char* path, uint16_t KeepAllIfShorterThan, uint8_t DesiredKeepElementsFront, uint8_t DesiredKeepElementsBack);
//The auto function is a simple wrapper and any random bullshit cases are handled by the non-auto function -> not covered by 0_st_tests
int AbbreviatePathAuto(char** ret, const char* path, uint16_t KeepAllIfShorterThan, uint8_t DesiredKeepElements);

#endif
