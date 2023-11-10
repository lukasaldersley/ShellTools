#ifndef __BVBS_ZSH_COMMONS__
#define __BVBS_ZSH_COMMONS__

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define COLOUR_GREYOUT "\e[38;5;240m"
#define COLOUR_CLEAR "\e[0m"
#define MODIFIER_BOLD "\e[1m"

typedef enum { ALPHA_BEFORE, ALPHA_EQUAL, ALPHA_AFTER } StringRelations;
bool Compare(const char* a, const char* b);
char ToLowerCase(char c);
char ToUpperCase(char c);
bool StartsWith(const char* a, const char* b);
bool ContainsString(const char* str, const char* test);
char* ExecuteProcess(const char* command);
int strlen_visible(const char* s);
void TerminateStrOn(char* str, const char* terminators);
StringRelations CompareStrings(const char* a, const char* b);
int LastIndexOf(const char* txt, char tst);
char* AbbreviatePathAuto(const char* path, int KeepAllIfShorterThan, int DesiredKeepElements);
char* AbbreviatePath(const char* path, int KeepAllIfShorterThan, int DesiredKeepElementsFront, int DesiredKeepElementsBack);
void abortNomem();

#endif
