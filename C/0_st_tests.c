#/*
TargetDir="$ST_CFG"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename "$0" .c).elf"
ThisFolder="$(realpath "$(dirname "$0")")"

if [ $# -eq 0 ]; then
	if command -v include-what-you-use > /dev/null 2>&1; then
		ST_IWYU_PATH="$ST_SRC/C/"
		printf "Running iwyu for %s" "$ST_IWYU_PATH*"
		find "$ST_IWYU_PATH" -type f -a \( -name "*.c" -o -name "*.cpp" -o -name "*.cc" \) -exec sh -c 'include-what-you-use -Xiwyu --verbose=1 -Xiwyu --error=1 "$1" 2> /dev/null; ST_CHK_TEMP_RES="$?"; if [ "$ST_CHK_TEMP_RES" -ne 0 ] ; then printf "\n\n\e[38;5;001mIWYU MESSAGES FOR %s\e[0m\n\e[3m" "$1" ; include-what-you-use -Xiwyu --max_line_length=512 -Xiwyu --update_comments -Xiwyu --quoted_includes_first -Xiwyu --verbose=1 "$1" ; printf "\e[0m\e[38;5;130mEND IWYU MESSAGES FOR %s\e[0m\n" "$1"; fi' _ {} \;
		printf " -> DONE\n"
		unset ST_IWYU_PATH
	else
		echo "iwyu (include-what-you-use) unavailable, skipping"
	fi

	if command -v shellcheck > /dev/null 2>&1; then
		ST_Test_ShellCheck_Base="$ST_SRC"
		if [ -e "$ST_SRC/../ShellToolsExtensionLoader.sh" ]; then
			#If an extension-loader exists, check it and other extension files as well.
			ST_Test_ShellCheck_Base="$(realpath "$ST_SRC/../")"
		fi
		printf "Running Shellcheck on %s" "$ST_Test_ShellCheck_Base"
		find "$ST_Test_ShellCheck_Base" -type f \( -name '*.sh' -o -name '*.zsh-theme' \) -execdir shellcheck -x "{}" \;
		printf " -> DONE\n"
		unset ST_Test_ShellCheck_Base
	else
		echo "shellcheck unavailable, skipping"
	fi

	if command -v cppcheck > /dev/null 2>&1; then
		printf "Running cppcheck on %s*" "$ThisFolder/"
		cppcheck --suppress=missingIncludeSystem --enable=all --inconclusive --library=gnu --suppress=checkersReport --std=c23 --inline-suppr --check-level=exhaustive --error-exitcode=1 --quiet "$ThisFolder" > /dev/null 2>&1
		if [ $? -eq 1 ]; then
			printf " -> \e[31mFAIL\e[0m\ncppcheck found Problems:\n"
			cppcheck --suppress=missingIncludeSystem --enable=all --inconclusive --library=gnu --suppress=checkersReport --std=c23 --inline-suppr --check-level=exhaustive --error-exitcode=1 "$ThisFolder"
		else
			printf " -> \e[32mPASS\e[0m\n"
		fi
	else
		echo "cppcheck unavailable, skipping"
	fi
else
	printf "%s was called with parameters (%s) => skipping all external checks\n" "$0" "$*"
fi

printf "compiling %s into %s/%s" "$0" "$TargetDir" "$TargetName"
#cppcheck is configured to use C23, while for gcc I use c2x, which is essentially the same. c2x is the older, deprecated name for c23.
#gcc-13 (used in ubuntu 24.04 LTS, which I do want to support) doesn't understand -std=c23, only -stx=c2x, the c23 flag was added with gcc-14
gcc -O3 -std=c2x -Wall -DDISABLE_EXPECTED_WARNING_FOR_INTERNAL_TESTS "$ThisFolder/commons.c" "$0" -o "$TargetDir/$TargetName" "$@"
#I WANT to be able to do things like ./shelltoolsmain.c -DPROFILING to add the compiler flag profiling but ALSO stuff like ./shelltoolsmain "-DDEBUG -DPROFILING" to add both profiling and debug
ST_RC="$?"
printf " -> \e[32mDONE\e[0m(%s)\n" "$ST_RC"
if [ "$ST_RC" -eq 0 ]; then
	unset ST_RC
	"$TargetDir/$TargetName"
	if [ $? -eq 0 ]; then
		printf "Tests in %s completed \e[32mPASS\e[0m\n" "$TargetDir/$TargetName"
		exit 0
	else
		printf "Tests in %s completed \e[31mFAIL\e[0m\n" "$TargetDir/$TargetName"
		exit 1
	fi
else
	printf "Compilation of %s \e[31mFAILED\e[0m (%s)\n\e[33mNO TESTS EXECUTED\e[0m\n" "$0" "$ST_RC"
	exit "$ST_RC"
fi
*/

#include "commons.h" // for Compare, AbbreviatePath, NextIndexOf, StartsWith, LastIndexOf, ToLowerCase, ABORT_NO_MEMORY

#include <stdbool.h> // for false, true, bool
#include <stdio.h> // for printf, NULL, asprintf
#include <stdlib.h> // for free
#include <string.h> // for strlen

//Assumption prior to calling this following variables exist in the local scope:
//bool TestsPass = true; int TestNo = 0;
#define TestWithExpectation(expectedResult, function, arguments...) TestNo++; if(! (function(arguments) == (expectedResult)) ) { TestPass = false; printf("\nTest of %1$s Number %2$i (%3$s:%4$i) [%1$s(%5$s) == "#expectedResult"] failed", #function, TestNo, __FILE__, __LINE__, #arguments); };

//Assumption prior to calling this following variables exist in the local scope:
//bool TestsPass = true; int TestNo = 0; int AvLen;
#define TestComboWithExpectation(expectedResult, sumSpace, function, arguments...) TestNo++; AvLen = sumSpace; if(! (function(&AvLen, arguments) == (expectedResult)) ) { TestPass = false; printf("\nTest of %1$s Number %2$i (%3$s:%4$i) [%1$s(&AvLen, %5$s) == "#expectedResult"] failed", #function, TestNo, __FILE__, __LINE__,#arguments); };

//Assumption prior to calling this following variables exist in the local scope:
//bool TestsPass = true; int TestNo = 0;
#define TestAbrevPath(retcode, expectedResult, function, arguments...) TestNo++; { char* TestDestBuffer; int TestRetCode = function(&TestDestBuffer, arguments); if ( !(Compare(TestDestBuffer, expectedResult) && TestRetCode == retcode) ) { TestPass = false; printf("\nTest of "#function" Number %i (%s:%i) ["#function"(&TestDestBuffer, "#arguments")] failed {Expected ["#retcode","#expectedResult"] but got [%i,\"%s\"]}", TestNo, __FILE__, __LINE__, TestRetCode, TestDestBuffer); }; free(TestDestBuffer); } ;

//Assumption prior to calling this following variables exist in the local scope:
//bool TestsPass = true; int TestNo = 0; StrlenStruct s;
#define TestStrlenWithExpectation(expectedReturnCode, expectedHeight, expectedLength, function, arguments...) TestNo++; s = (StrlenStruct) {.len = 0, .height = 0}; if (!(function(&s, arguments) == (expectedReturnCode) && expectedHeight == s.height && expectedLength == s.len)) { TestPass = false; /*NOTE: I have doubled up the #function parameter, since cppcheck completely looses it's little mind with positional arguments*/printf("\nTest of %s Number %i (%s:%i) [%s(&s, %s) == "#expectedReturnCode"] with additional constraint expect{%i, %i} == have{%i, %i} failed", #function, TestNo, __FILE__, __LINE__, #function, #arguments, expectedLength, expectedHeight, s.len, s.height); };

#ifdef MANUAL
static void TestManual() {
	//INSERT TESTS HERE
}
#endif

#pragma region AutomatedTests
//Commons: Immutable Functions
static bool TestCommonsCompare() {
	char* ForceNoSamePointer;
	if (asprintf(&ForceNoSamePointer, "Hello World!") == -1) ABORT_NO_MEMORY;
	int TestNo = 0;
	bool TestPass = true;
	TestWithExpectation(false, Compare, "Hello World!", "Hello");
	//Attention: the line below may be optimized to pass the same pointer into the function, since the strings are const and thus a single string in .bss
	TestWithExpectation(true, Compare, "Hello World!", "Hello World!");
	TestWithExpectation(true, Compare, "Hello World!", ForceNoSamePointer);
	TestWithExpectation(false, Compare, "Hello World!", "Hello World! (of Testing)");
	TestWithExpectation(false, Compare, "Hello World!", "He.lo World!");
	TestWithExpectation(false, Compare, NULL, "Hello World!");
	TestWithExpectation(false, Compare, "Hello World!", NULL);
	TestWithExpectation(true, Compare, NULL, NULL);
	//if the pointers are the same, the result is true, no matter what the pointers are, here I just misuse TestNo and treat it's pointer as char since it'll never be dereferenced
	TestWithExpectation(true, Compare, (char*)&TestNo, (char*)&TestNo);
	free(ForceNoSamePointer);
	ForceNoSamePointer = NULL;
	return TestPass;
}

StringRelations CompareStringsLexicographical(const char* a, const char* b); //TODO REMOVE ONCE PROPERLY IMPLEMENTED
static bool TestCommonsCompareStrings() {
	char* ForceNoSamePointer;
	if (asprintf(&ForceNoSamePointer, "Hello World!") == -1) ABORT_NO_MEMORY;
	int TestNo = 0;
	bool TestPass = true;
	TestWithExpectation(ALPHA_EQUAL, CompareStringsLexicographical, "Hello World!", "Hello World!");
	TestWithExpectation(ALPHA_EQUAL, CompareStringsCaseInsensitive, "Hello World!", "Hello World!");
	TestWithExpectation(ALPHA_EQUAL, CompareStringsAsciiByteValues, "Hello World!", "Hello World!");
	TestWithExpectation(ALPHA_AFTER, CompareStringsLexicographical, "Hello World!", "Hello World");
	TestWithExpectation(ALPHA_AFTER, CompareStringsCaseInsensitive, "Hello World!", "Hello World");
	TestWithExpectation(ALPHA_AFTER, CompareStringsAsciiByteValues, "Hello World!", "Hello World");
	TestWithExpectation(ALPHA_BEFORE, CompareStringsLexicographical, "Hello World", "Hello World!");
	TestWithExpectation(ALPHA_BEFORE, CompareStringsCaseInsensitive, "Hello World", "Hello World!");
	TestWithExpectation(ALPHA_BEFORE, CompareStringsAsciiByteValues, "Hello World", "Hello World!");
	TestWithExpectation(ALPHA_EQUAL, CompareStringsLexicographical, "Hello World!", ForceNoSamePointer);
	TestWithExpectation(ALPHA_EQUAL, CompareStringsCaseInsensitive, "Hello World!", ForceNoSamePointer);
	TestWithExpectation(ALPHA_EQUAL, CompareStringsAsciiByteValues, "Hello World!", ForceNoSamePointer);
	TestWithExpectation(ALPHA_EQUAL, CompareStringsLexicographical, ForceNoSamePointer, "Hello World!");
	TestWithExpectation(ALPHA_EQUAL, CompareStringsCaseInsensitive, ForceNoSamePointer, "Hello World!");
	TestWithExpectation(ALPHA_EQUAL, CompareStringsAsciiByteValues, ForceNoSamePointer, "Hello World!");
	TestWithExpectation(ALPHA_AFTER, CompareStringsLexicographical, "Hello World!", NULL);
	TestWithExpectation(ALPHA_AFTER, CompareStringsCaseInsensitive, "Hello World!", NULL);
	TestWithExpectation(ALPHA_AFTER, CompareStringsAsciiByteValues, "Hello World!", NULL);
	TestWithExpectation(ALPHA_BEFORE, CompareStringsLexicographical, NULL, "Hello World!");
	TestWithExpectation(ALPHA_BEFORE, CompareStringsCaseInsensitive, NULL, "Hello World!");
	TestWithExpectation(ALPHA_BEFORE, CompareStringsAsciiByteValues, NULL, "Hello World!");
	TestWithExpectation(ALPHA_EQUAL, CompareStringsLexicographical, NULL, NULL);
	TestWithExpectation(ALPHA_EQUAL, CompareStringsCaseInsensitive, NULL, NULL);
	TestWithExpectation(ALPHA_EQUAL, CompareStringsAsciiByteValues, NULL, NULL);
	//if the pointers are the same, the result is true, no matter what the pointers are, here I just misuse TestNo and treat it's pointer as char since it'll never be dereferenced
	TestWithExpectation(ALPHA_EQUAL, CompareStringsLexicographical, (char*)&TestNo, (char*)&TestNo);
	TestWithExpectation(ALPHA_EQUAL, CompareStringsCaseInsensitive, (char*)&TestNo, (char*)&TestNo);
	TestWithExpectation(ALPHA_EQUAL, CompareStringsAsciiByteValues, (char*)&TestNo, (char*)&TestNo);
	//implement more tests, also unicode, also do I want z<A or a<A&&z>A
	free(ForceNoSamePointer);
	ForceNoSamePointer = NULL;
	return TestPass;
}

static bool TestCommonsStartsWith() {
	char* ForceNoSamePointer;
	if (asprintf(&ForceNoSamePointer, "Hello World!") == -1) ABORT_NO_MEMORY;
	int TestNo = 0;
	bool TestPass = true;
	TestWithExpectation(true, StartsWith, "Hello World!", "Hello");
	//Attention: the line below may be optimized to pass the same pointer into the function, since the strings are const and thus a single string in .bss
	TestWithExpectation(true, StartsWith, "Hello World!", "Hello World!");
	//This is basically the inverse of the one above. I force the pointers to be different (one on heap, the other in bss) but they contain the same test
	TestWithExpectation(true, StartsWith, "Hello World!", ForceNoSamePointer);
	TestWithExpectation(false, StartsWith, "Hello World!", "Hello World! (of Testing)");
	TestWithExpectation(true, StartsWith, "Hello World! (of Testing)", "Hello World!");
	TestWithExpectation(false, StartsWith, "Hello World!", "He.lo World!");
	TestWithExpectation(false, StartsWith, NULL, "Hello World!");
	TestWithExpectation(false, StartsWith, "Hello World!", NULL);
	TestWithExpectation(true, StartsWith, NULL, NULL);
	//if the pointers are the same, the result is true, no matter what the pointers are, here I just misuse TestNo and treat it's pointer as char since it'll never be dereferenced
	TestWithExpectation(true, StartsWith, (char*)&TestNo, (char*)&TestNo);
	free(ForceNoSamePointer);
	ForceNoSamePointer = NULL;
	return TestPass;
}

static bool TestCommonsContainsString() {
	const char* teststring = "/some/string/0/~string~/";
	int TestNo = 0;
	bool TestPass = true;
	TestWithExpectation(true, ContainsString, NULL, NULL);
	TestWithExpectation(true, ContainsString, teststring, teststring);
	//Attention: the line below may be optimized to pass the same pointer into the function, since the strings are const and thus a single string in .bss
	TestWithExpectation(true, ContainsString, "/some/string/0/~string~/", "/some/string/0/~string~/");
	//This is basically the inverse of the one above. I force the pointers to be different (one on heap, the other in bss) but they contain the same test
	TestWithExpectation(true, ContainsString, "/some/string/0/~string~/", teststring);
	TestWithExpectation(false, ContainsString, NULL, "Hello World!");
	TestWithExpectation(false, ContainsString, "Hello World!", NULL);
	TestWithExpectation(true, ContainsString, (char*)&TestNo, (char*)&TestNo);
	TestWithExpectation(false, ContainsString, "FUCK", "fucker");
	TestWithExpectation(false, ContainsString, "Hello", "Hello World!");
	TestWithExpectation(true, ContainsString, "Hello World!", "Hello");
	TestWithExpectation(true, ContainsString, "Hello World!", "World!");
	TestWithExpectation(false, ContainsString, "Hello", "Hello World!");
	TestWithExpectation(false, ContainsString, "World!", "Hello World!");
	TestWithExpectation(false, ContainsString, "", "Hello World!");
	TestWithExpectation(true, ContainsString, "Hello World!", "");
	TestWithExpectation(true, ContainsString, "Hello World!", "llo Wor");
	TestWithExpectation(false, ContainsString, "llo Wor", "Hello World!");
	return TestPass;
}

static bool TestCommonsLastIndexOf() {
	const char* teststring = "/some/string/0/~string~/";
	int TestNo = 0;
	bool TestPass = true;
	TestWithExpectation(23, LastIndexOf, teststring, '/');
	TestWithExpectation(4, LastIndexOf, teststring, 'e');
	TestWithExpectation(-1, LastIndexOf, teststring, 'z');
	TestWithExpectation(13, LastIndexOf, teststring, '0');
	TestWithExpectation(24, LastIndexOf, teststring, 0);
	TestWithExpectation((int)strlen(teststring), LastIndexOf, teststring, 0);
	TestWithExpectation((int)strlen(teststring), LastIndexOf, teststring, 0x00);
	TestWithExpectation(-2, LastIndexOf, NULL, 0);
	TestWithExpectation(-2, LastIndexOf, NULL, 'a');
	return TestPass;
}

static bool TestCommonsNextIndexOf() {
	const char* teststring = "/some/string/0/~string~/";
	int TestNo = 0;
	bool TestPass = true;
	TestWithExpectation(0, NextIndexOf, teststring, '/', 0);
	TestWithExpectation(5, NextIndexOf, teststring, '/', 1);
	TestWithExpectation(-1, NextIndexOf, teststring, 'z', 0);
	TestWithExpectation(-2, NextIndexOf, teststring, '/', 100);
	TestWithExpectation(-2, NextIndexOf, teststring, 'z', 100);
	TestWithExpectation(13, NextIndexOf, teststring, '0', 0);
	TestWithExpectation(13, NextIndexOf, teststring, '0', 13);
	TestWithExpectation(-1, NextIndexOf, teststring, '0', 14);
	TestWithExpectation(-2, NextIndexOf, teststring, '/', -1);
	TestWithExpectation(-2, NextIndexOf, NULL, 0, 0);
	return TestPass;
}

static bool TestCommonsStrlen_visible() {
	int TestNo = 0;
	bool TestPass = true;
	StrlenStruct s;
	char* TempBuf = malloc(sizeof(char) * 2);
	if (TempBuf == NULL) ABORT_NO_MEMORY;
	TempBuf[1] = 0x00;
	for (int i = 0x00; i < 0x20; i++) {
		TempBuf[0] = (char)i;
		if (i == '\t') {
			TestStrlenWithExpectation(true, 1, 4, strlen_visible_config, TempBuf, false);
			TestStrlenWithExpectation(true, 1, 4, strlen_visible_config, TempBuf, true);
		} else if (i == '\v' || i == '\f' || i == '\n') {
			TestStrlenWithExpectation(true, 2, 0, strlen_visible_config, TempBuf, false);
			TestStrlenWithExpectation(true, 2, 0, strlen_visible_config, TempBuf, true);
		} else {
			TestStrlenWithExpectation(true, 1, 0, strlen_visible_config, TempBuf, false);
			TestStrlenWithExpectation(true, 1, 0, strlen_visible_config, TempBuf, true);
		}
	}
	for (int i = 0x20; i < 0x7f; i++) {
		TempBuf[0] = (char)i;
		TestStrlenWithExpectation(true, 1, 1, strlen_visible_config, TempBuf, false);
		TestStrlenWithExpectation(true, 1, 1, strlen_visible_config, TempBuf, true);
	}
	TestStrlenWithExpectation(true, 1, 0, strlen_visible_config, "\x7f", false);
	TestStrlenWithExpectation(true, 1, 0, strlen_visible_config, "\x7f", true);
	TestStrlenWithExpectation(true, 1, 0, strlen_visible_config, "", false);
	TestStrlenWithExpectation(true, 1, 0, strlen_visible_config, "", true);
	TestStrlenWithExpectation(false, 0, 0, strlen_visible_config, NULL, false);
	TestStrlenWithExpectation(true, 1, 1, strlen_visible_config, "A", false);
	TestStrlenWithExpectation(true, 1, 1, strlen_visible_config, "A", true);
	TestStrlenWithExpectation(true, 1, 2, strlen_visible_config, "%b", false);
	TestStrlenWithExpectation(true, 1, 0, strlen_visible_config, "%b", true);
	TestStrlenWithExpectation(true, 1, 2, strlen_visible_config, "%%", false);
	TestStrlenWithExpectation(true, 1, 1, strlen_visible_config, "%%", true);
	TestStrlenWithExpectation(true, 1, 1, strlen_visible_config, "€", false);
	TestStrlenWithExpectation(true, 1, 12, strlen_visible_config, "\e[1mäÄöÖüÜßẞ€°§´\e[0m", false);
	TestStrlenWithExpectation(true, 1, 12, strlen_visible_config, "\e[1mäÄöÖüÜßẞ€°§´\e[0m\r", false);
	TestStrlenWithExpectation(true, 1, 12, strlen_visible_config, "\e[1mäÄöÖüÜßẞ€°§´\e[0m\rblahblahblah", false);
	TestStrlenWithExpectation(true, 1, 12, strlen_visible_config, "\e[1mäÄöÖüÜßẞ€°§´\e[0m\rblahblah", false);
	TestStrlenWithExpectation(true, 1, 12, strlen_visible_config, "\e[1mäÄöÖüÜß\e[0m\rblahblahblah", false);
	TestStrlenWithExpectation(true, 1, 12, strlen_visible_config, "\e[1mäÄöÖüÜßẞ€°§´\e[0m\rblah", false);
	TestStrlenWithExpectation(true, 1, 17, strlen_visible_config, "\e[1mäÄöÖüÜßẞ€°§´\e[0m\rblahblahblahHello", false);
	TestStrlenWithExpectation(true, 2, 24, strlen_visible_config, "\e[1mäÄöÖüÜßẞ€°§´\e[0m\vblahblahblah", false);
	TestStrlenWithExpectation(true, 2, 24, strlen_visible_config, "\e[1mäÄöÖüÜßẞ€°§´\e[0m\fblahblahblah", false);
	TestStrlenWithExpectation(true, 1, 2, strlen_visible_config, "\e0m", false);
	TestStrlenWithExpectation(true, 1, 0, strlen_visible_config, "\e[0m", false);
	return TestPass;
}

static bool TestCommonsDeterminatePossibleCombinations() {
	int TestNo = 0;
	bool TestPass = true;
	int AvLen = 0;
	TestComboWithExpectation(0b0000000000000000, 0, determinePossibleCombinations, 0);
	TestComboWithExpectation(0b0000000000000000, 0, determinePossibleCombinations, 0, 5, 7, 6, 3, 4, 2, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1);
	TestComboWithExpectation(0b0000000000101001, 10, determinePossibleCombinations, 16, 5, 7, 6, 3, 4, 2, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1);
	TestComboWithExpectation(0b1111111111111111, 100, determinePossibleCombinations, 16, 5, 7, 6, 3, 4, 2, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1);
	TestComboWithExpectation(0b0111111111111111, 100, determinePossibleCombinations, 15, 5, 7, 6, 3, 4, 2, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1);
	TestComboWithExpectation(0b0011111111111111, 100, determinePossibleCombinations, 14, 5, 7, 6, 3, 4, 2, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1);
	TestComboWithExpectation(0b0000000010000111, 19, determinePossibleCombinations, 16, 5, 7, 6, 3, 4, 2, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1);
	TestComboWithExpectation(0b0000000010101111, 24, determinePossibleCombinations, 16, 5, 7, 6, 3, 4, 2, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1);
	TestComboWithExpectation(0b1111111111111111, 44, determinePossibleCombinations, 16, 5, 7, 6, 3, 4, 2, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1);
	TestComboWithExpectation(0b0111111111111111, 43, determinePossibleCombinations, 16, 5, 7, 6, 3, 4, 2, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1);
	TestComboWithExpectation(0b0000111111111111, 40, determinePossibleCombinations, 16, 5, 7, 6, 3, 4, 2, 8, 1, 1, 1, 1, 1, 1, 1, 1, 1);
	return TestPass;
}

static bool TestCommonsToLowerCase() {
	//TODO rework both function and test
	bool TestPass = true;
	for (char i = 0; i < 'A'; i++) {
		if (i != ToLowerCase(i)) {
			TestPass = false;
			printf("TestToLowerCase Number %i failed\n", i);
		}
	}
	for (char i = 'A'; i <= 'Z'; i++) {
		if ((i + 0x20) != ToLowerCase(i)) {
			TestPass = false;
			printf("TestToLowerCase Number %i failed\n", i);
		}
	}
	for (char i = ('Z' + 1); i <= 0x7f; i++) {
		if (i != ToLowerCase(i)) {
			TestPass = false;
			printf("TestToLowerCase Number %i failed [%c/%c]\n", i, i, ToLowerCase(i));
		}
		if (i == 0x7f) break; //to allow the full range to be tested
	}
	return TestPass;
}

static bool TestCommonsToUpperCase() {
	//TODO rework function and implement test
	return true;
}

//Commons: Mutable Functions
static bool TestCommonsTerminateStrOn() {
	return true;
}

static bool TestCommonsTerminateThenTrimStrOn() {
	return true;
}

static bool TestCommonsParseCharOrCodePoint() {
	return true;
}

static bool TestCommonsCopyStringNumCharConfig() {
	return true;
}

static bool TestCommonsAbbreviatePath() {
	bool TestPass = true;
	int TestNo = 0;

	TestAbrevPath(0, "[...]", AbbreviatePath, "/test1/", 5, 0, 0);
	TestAbrevPath(0, "[...]1", AbbreviatePath, "/testx1/", 6, 0, 0);
	TestAbrevPath(1, "", AbbreviatePath, NULL, 0, 0, 0);
	TestAbrevPath(1, "", AbbreviatePath, NULL, 0, 0, 1);
	TestAbrevPath(1, "", AbbreviatePath, NULL, 0, 1, 0);
	TestAbrevPath(1, "", AbbreviatePath, NULL, 1, 0, 0);
	TestAbrevPath(0, "", AbbreviatePath, "/test/", 0, 0, 0);
	TestAbrevPath(0, "/test", AbbreviatePath, "/test/", 0, 1, 0);
	TestAbrevPath(0, "/test", AbbreviatePath, "/test/", 0, 1, 1);
	TestAbrevPath(0, "/test", AbbreviatePath, "/test/", 0, 0, 1);
	TestAbrevPath(0, "!", AbbreviatePath, "/test/", 1, 0, 0);
	TestAbrevPath(0, "!!", AbbreviatePath, "/test/", 2, 0, 0);
	TestAbrevPath(0, "!!!", AbbreviatePath, "/test/", 3, 0, 0);
	TestAbrevPath(0, "!!!!", AbbreviatePath, "/test/", 4, 0, 0);
	TestAbrevPath(0, "/test", AbbreviatePath, "/test/", 5, 0, 0);
	TestAbrevPath(0, "[...]", AbbreviatePath, "/test1/", 5, 0, 0);
	TestAbrevPath(0, "/test1", AbbreviatePath, "/test1/", 6, 0, 0);
	TestAbrevPath(0, "[...]1", AbbreviatePath, "/testx1/", 6, 0, 0);
	TestAbrevPath(0, "/test", AbbreviatePath, "/test/", 6, 0, 0);
	TestAbrevPath(0, "/test", AbbreviatePath, "/test", 5, 0, 0);
	TestAbrevPath(0, "[...]", AbbreviatePath, "A_Reeeeaaallly_long_name_without_parent", 5, 0, 0);
	TestAbrevPath(0, "[...]t", AbbreviatePath, "A_Reeeeaaallly_long_name_without_parent", 6, 0, 0);
	TestAbrevPath(0, "A_Reeeeaaallly_long_name_without_parent", AbbreviatePath, "A_Reeeeaaallly_long_name_without_parent", 5, 0, 1);
	TestAbrevPath(0, "A_Reeeeaaallly_long_name_without_parent", AbbreviatePath, "A_Reeeeaaallly_long_name_without_parent", 5, 1, 0);
	TestAbrevPath(0, "A_Reeeeaaallly_long_name_without_parent", AbbreviatePath, "A_Reeeeaaallly_long_name_without_parent", 5, 1, 1);
	TestAbrevPath(0, "A_Reeeeaaallly_long_name_without_parent/[...]/child", AbbreviatePath, "A_Reeeeaaallly_long_name_without_parent/but/with/child", 10, 1, 1);
	TestAbrevPath(0, "A_Reeeeaaallly_long_name_without_parent/but/with_child", AbbreviatePath, "A_Reeeeaaallly_long_name_without_parent/but/with_child", 10, 1, 1);
	TestAbrevPath(0, "/", AbbreviatePath, "/", 1, 0, 0);
	TestAbrevPath(0, "/", AbbreviatePath, "////", 1, 0, 0);
	TestAbrevPath(0, "/", AbbreviatePath, "////", 10, 0, 0);
	TestAbrevPath(0, "/test/now", AbbreviatePath, "/test/now", 10, 0, 0);
	TestAbrevPath(0, "/test/now1", AbbreviatePath, "/test/now1", 10, 0, 0);
	TestAbrevPath(0, "[...]now12", AbbreviatePath, "/test/now12", 10, 0, 0);
	TestAbrevPath(0, "[...]t/now", AbbreviatePath, "/some/path/to/test/now", 10, 0, 0);
	TestAbrevPath(0, "/[...]/now", AbbreviatePath, "/some/path/to/test/now", 10, 0, 1);
	TestAbrevPath(0, "/[...]/nowbutTooLong", AbbreviatePath, "/some/path/to/test/nowbutTooLong", 10, 0, 1);
	TestAbrevPath(0, "[...]/now", AbbreviatePath, "some/path/to/test/now", 10, 0, 1);
	TestAbrevPath(0, "/some/[...]", AbbreviatePath, "/some/path/to/test/now", 10, 1, 0);
	TestAbrevPath(0, "/some/[...]/now", AbbreviatePath, "/some/path/to/test/now", 10, 1, 1);
	TestAbrevPath(0, "some/path/[...]/now", AbbreviatePath, "some/path/to/test/now", 10, 2, 1);
	TestAbrevPath(0, "/some/path/[...]/now", AbbreviatePath, "/some/path/to/test/now", 10, 2, 1);
	TestAbrevPath(0, "/some/[...]/test/now", AbbreviatePath, "/some/path/to/test/now", 10, 1, 2);
	TestAbrevPath(0, "/some/path/to/test/now", AbbreviatePath, "/some/path/to/test/now", 10, 2, 2);
	TestAbrevPath(0, "/some/path/to3/test/now", AbbreviatePath, "/some/path/to3/test/now", 10, 2, 2);
	TestAbrevPath(0, "/some/path/to34/test/now", AbbreviatePath, "/some/path/to34/test/now", 10, 2, 2);
	TestAbrevPath(0, "/some/path/to345/test/now", AbbreviatePath, "/some/path/to345/test/now", 10, 2, 2);
	TestAbrevPath(0, "/some/path/[...]/test/now", AbbreviatePath, "/some/path/to3456/test/now", 10, 2, 2);
	TestAbrevPath(0, "/some/path/[...]/test/now", AbbreviatePath, "/some/path/to34567/test/now", 10, 2, 2);
	TestAbrevPath(0, "/some/path/to34567/test/now", AbbreviatePath, "/some/path/to34567/test/now", 100, 2, 2);
	TestAbrevPath(0, "/some/path/[...]/test/now", AbbreviatePath, "/some/path/to34567/test/now", 1, 2, 2);
	return TestPass;
}
#pragma endregion

int main() {

#ifdef MANUAL
	//this is only executed in DEBUG and is essentially my local testing playground
	printf("Running MANUAL Tests\n--------------------------------\n");
	TestManual();
	printf("--------------------------------\nCompleted MANUAL Tests\n");
#endif
	bool TestPass = true;
	printf("Executing Tests for commons.c -> ");
	TestPass = TestPass && TestCommonsCompare();
	TestPass = TestPass && TestCommonsCompareStrings();
	TestPass = TestPass && TestCommonsStartsWith();
	TestPass = TestPass && TestCommonsContainsString();
	TestPass = TestPass && TestCommonsLastIndexOf();
	TestPass = TestPass && TestCommonsNextIndexOf();
	TestPass = TestPass && TestCommonsStrlen_visible();
	TestPass = TestPass && TestCommonsTerminateStrOn();
	TestPass = TestPass && TestCommonsTerminateThenTrimStrOn();
	TestPass = TestPass && TestCommonsCopyStringNumCharConfig();
	TestPass = TestPass && TestCommonsToLowerCase();
	TestPass = TestPass && TestCommonsToUpperCase();
	TestPass = TestPass && TestCommonsAbbreviatePath();
	TestPass = TestPass && TestCommonsDeterminatePossibleCombinations();
	TestPass = TestPass && TestCommonsParseCharOrCodePoint();

	if (TestPass) {
		printf("\e[32mPASS\e[0m\n");
		return 0;
	} else {
		printf("\n\e[31mFAIL\e[0m\n");
		return 1;
	}
}
