#ifndef __BVBS_ZSH_COMMONS__
#define __BVBS_ZSH_COMMONS__

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#define COLOUR_GREYOUT "\e[38;5;240m"
#define COLOUR_CLEAR "\e[0m"
#define MODIFIER_BOLD "\e[1m"
#define true 1
#define false 0

#define DEFAULT_TERMINATORS "\r\n\a"

#define ABORT_NO_MEMORY {abortNomem();exit(1);}

typedef enum { ALPHA_BEFORE, ALPHA_EQUAL, ALPHA_AFTER } StringRelations;

void abortNomem();
void abortMessage(const char* message);

bool Compare(const char* a, const char* b);
StringRelations CompareStrings(const char* a, const char* b);
bool StartsWith(const char* a, const char* b);
bool ContainsString(const char* str, const char* test);
int16_t LastIndexOf(const char* txt, char tst);
int16_t NextIndexOf(const char* txt, char tst, int startindex);
int strlen_visible(const char* s);
uint32_t TerminateStrOn(char* str, const char* terminators);
int cpyString(char* dest, const char* src, int maxCount);
char ToLowerCase(const char c);
char ToUpperCase(const char c);
char* ExecuteProcess_alloc(const char* command);
int AbbreviatePathAuto(char** ret, const char* path, uint16_t KeepAllIfShorterThan, uint8_t DesiredKeepElements);
int AbbreviatePath(char** ret, const char* path, uint16_t KeepAllIfShorterThan, uint8_t DesiredKeepElementsFront, uint8_t DesiredKeepElementsBack);
uint32_t determinePossibleCombinations(int* availableLength, int NumElements, ...);
#endif
