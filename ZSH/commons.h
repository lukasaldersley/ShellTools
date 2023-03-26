#ifndef __BVBS_ZSH_COMMONS__
#define __BVBS_ZSH_COMMONS__

#define _GNU_SOURCE
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

bool Compare(const char* a, const char* b);
char ToLowerCase(char c);
char ToUpperCase(char c);
bool StartsWith(const char* a, const char* b);
bool ContainsString(const char* str, const char* test);
char* ExecuteProcess(const char* command);
int strlen_visible(const char* s);
void TerminateStrOn(char* str, const char* terminators);

#endif
