#/*
TargetDir="$ST_CFG"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename "$0" .c).elf"
ThisFolder="$(realpath "$(dirname "$0")")"

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
if [ $# -eq 0 ] && command -v cppcheck > /dev/null 2>&1; then
	#if this is called without arguments, run cppcheck on the entire folder as an additional release check
	printf "Running cppcheck on %s*" "$ThisFolder/"
	cppcheck --suppress=missingIncludeSystem --enable=all --inconclusive --library=gnu --suppress=checkersReport --std=c11 --inline-suppr --check-level=exhaustive --error-exitcode=1 --quiet "$ThisFolder" > /dev/null 2>&1
	if [ $? -eq 1 ]; then
		printf " -> \e[31mFAIL\e[0m\ncppcheck found Problems:"
		cppcheck --suppress=missingIncludeSystem --enable=all --inconclusive --library=gnu --suppress=checkersReport --std=c11 --inline-suppr --check-level=exhaustive --error-exitcode=1 "$ThisFolder"
	else
		printf " -> \e[32mPASS\e[0m\n"
	fi
else
	echo "Skipping cppcheck due to arguments being present or because it isn't installed"
fi
printf "compiling %s into %s/%s" "$0" "$TargetDir" "$TargetName"
#shellcheck disable=SC2086 #in this case I DO want word splitting to happen at $1
gcc -O3 -std=c2x -Wall "$ThisFolder/commons.c" "$0" -o "$TargetDir/$TargetName" $1
#I WANT to be able to do things like ./repotools.c -DPROFILING to add the compiler flag profiling but ALSO stuff like ./repotools "-DDEBUG -DPROFILING" to add both profiling and debug
printf " -> \e[32mDONE\e[0m(%s)\n" $?
"$TargetDir/$TargetName"
if [ $? -eq 0 ]; then
	printf "Tests in %s completed \e[32mPASS\e[0m\n" "$TargetDir/$TargetName"
	exit 0
else
	printf "Tests in %s completed \e[31mFAIL\e[0m\n" "$TargetDir/$TargetName"
	exit 1
fi
*/

#include "commons.h"
#include <stdio.h>
#include <assert.h>

#pragma region AutomatedTests
bool TestStartsWith() {
	char* ForceNoSamePointer;
	if (asprintf(&ForceNoSamePointer, "Hello World!") == -1) ABORT_NO_MEMORY;
	int TestNo = 0;
	bool testPass = true;
	if (!StartsWith("Hello World!", "Hello") == true) { testPass = false;printf("TestStartsWith Number %i failed\n", TestNo); }TestNo++;
	//Attention: the line below may be optimized to pass the same pointer into the function, since the strings are const and thus a single string in .bss
	if (!StartsWith("Hello World!", "Hello World!") == true) { testPass = false;printf("TestStartsWith Number %i failed\n", TestNo); }TestNo++;
	if (!StartsWith("Hello World!", ForceNoSamePointer) == true) { testPass = false;printf("TestStartsWith Number %i failed\n", TestNo); }TestNo++;
	if (!StartsWith("Hello World!", "Hello World! (of Testing)") == false) { testPass = false;printf("TestStartsWith Number %i failed\n", TestNo); }TestNo++;
	if (!StartsWith("Hello World!", "He.lo World!") == false) { testPass = false;printf("TestStartsWith Number %i failed\n", TestNo); }TestNo++;
	if (!StartsWith(NULL, "Hello World!") == false) { testPass = false;printf("TestStartsWith Number %i failed\n", TestNo); }TestNo++;
	if (!StartsWith("Hello World!", NULL) == false) { testPass = false;printf("TestStartsWith Number %i failed\n", TestNo); }TestNo++;
	if (!StartsWith(NULL, NULL) == true) { testPass = false;printf("TestStartsWith Number %i failed\n", TestNo); }TestNo++;
	//if the pointers are the same, the result is true, no matter what the pointers are, here I just misuse TestNo and treat it's pointer as char since it'll never be dereferenced
	if (!StartsWith((char*)&TestNo, (char*)&TestNo) == true) { testPass = false;printf("TestStartsWith Number %i failed\n", TestNo); }TestNo++;
	free(ForceNoSamePointer);
	ForceNoSamePointer = NULL;
	return testPass;
}

bool TestLastIndexOf() {
	const char* teststring = "/some/string/0/~string~/";
	int TestNo = 0;
	bool testPass = true;
	if (!(LastIndexOf(teststring, '/') == 23)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(LastIndexOf(teststring, 'e') == 4)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(LastIndexOf(teststring, 'z') == -1)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(LastIndexOf(teststring, 0) == 24)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(LastIndexOf(teststring, 0) == (int)strlen(teststring))) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(LastIndexOf(NULL, 0) == -2)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(LastIndexOf(NULL, 'a') == -2)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	return testPass;
}

bool TestNextIndexOf() {
	const char* teststring = "/some/string/0/~string~/";
	int TestNo = 0;
	bool testPass = true;
	if (!(NextIndexOf(teststring, '/', 0) == 0)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(NextIndexOf(teststring, '/', 1) == 5)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(NextIndexOf(teststring, 'z', 0) == -1)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(NextIndexOf(teststring, '/', 100) == -2)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(NextIndexOf(teststring, 'z', 100) == -2)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(NextIndexOf(teststring, '0', 0) == 13)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(NextIndexOf(teststring, '0', 13) == 13)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(NextIndexOf(teststring, '0', 14) == -1)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(NextIndexOf(teststring, '/', -1) == -2)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	if (!(NextIndexOf(NULL, 0, 0) == -2)) { testPass = false;printf("TestLastIndexOf Number %i failed\n", TestNo); }TestNo++;
	return testPass;
}

bool TestAbbreviatePath() {
	printf("%s\n\n", AbbreviatePath("/some/path/to/test/now", 10, 2, 2));
	printf("%s\n\n", AbbreviatePath("/some/path/to3/test/now", 10, 2, 2));
	printf("%s\n\n", AbbreviatePath("/some/path/to34/test/now", 10, 2, 2));
	printf("%s\n\n", AbbreviatePath("/some/path/to345/test/now", 10, 2, 2));
	printf("%s\n\n", AbbreviatePath("/some/path/to3456/test/now", 10, 2, 2));
	printf("%s\n\n", AbbreviatePath("/some/path/to34567/test/now", 10, 2, 2));
	return true;
}
#pragma endregion

int main() {

#ifdef MANUAL
	//this is only executed in DEBUG and is essentially my local testing playground
	printf("Running MANUAL Tests\n\n");

	TestAbbreviatePath();

	printf("\nCompleted MANUAL Tests\n");
#endif
	bool TestPass = true;
	printf("Executing Tests for commons.c -> ");
	TestPass = TestPass && TestStartsWith();
	TestPass = TestPass && TestNextIndexOf();
	TestPass = TestPass && TestLastIndexOf();
	if (TestPass) {
		printf("\e[32mPASS\e[0m\n");
		return 0;
	}
	else {
		printf("\e[31mFAIL\e[0m\n");
		return 1;
	}
}
