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
#define true 1
#define false 0

#define ABORT_NO_MEMORY {abortNomem();exit(1);}

typedef enum { ALPHA_BEFORE, ALPHA_EQUAL, ALPHA_AFTER } StringRelations;
bool Compare(const char* a, const char* b);
char ToLowerCase(const char c);
char ToUpperCase(const char c);
bool StartsWith(const char* a, const char* b);
bool ContainsString(const char* str, const char* test);
char* ExecuteProcess(const char* command);
int strlen_visible(const char* s);
uint32_t TerminateStrOn(char* str, const char* terminators);
StringRelations CompareStrings(const char* a, const char* b);
int16_t LastIndexOf(const char* txt, char tst);
int16_t NextIndexOf(const char* txt, char tst, int startindex);
char* AbbreviatePathAuto(const char* path, uint16_t KeepAllIfShorterThan, uint8_t DesiredKeepElements);
char* AbbreviatePath(const char* path, uint16_t KeepAllIfShorterThan, uint8_t DesiredKeepElementsFront, uint8_t DesiredKeepElementsBack);
void abortNomem();
void abortMessage(const char* message);

#endif
