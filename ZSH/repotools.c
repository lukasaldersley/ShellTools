#/*
outname="$(basename $0 .c).elf"
echo "compiling $0 into ~/$outname"
gcc -std=c2x "$0" -o ~/"$outname"
exit
*/

//this gets the base location via argv[1], the list of allowed repo types from argv[2], the new target designation in argv[3] and reads the relative repos from stdin
//if the current remote of a checked repo is in one of the allowed repos, it's origin neds to be changed to the new one
//if one of the argv's isn't supplied or argv[3] isn't a valid rarget repo, fail




#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

#define maxGroups 10
const char* repocombos[2][4] = { "none","local","internal","global", "/dev/null", "/data/repos","ssh://git@127.0.0.1/data/repos/","ssh://git@someprivateurl.de:1234/data/repos/" };
regex_t RemoteRepoRegex;
regmatch_t CapturedResults[maxGroups];

bool Compare(const char* a, const char* b) {
	bool matching = true;
	int idx = 0;
	while (matching && a[idx] != 0x00 && b[idx] != 0x00) {
		if (a[idx] != b[idx]) {
			matching = false;
			break;
		}
		idx++;
	}
	return matching;
}

char ToLowerCase(char c) {
	if (c >= 'A' && c <= 'Z') {
		return c + 0x20; //A=0x41, a=0x61
	}
	else {
		return c;
	}
}

char ToUpperCase(char c) {
	if (c >= 'a') {
		return c - 0x20;
	}
	else {
		return c;
	}
}

bool StartsWith(const char* a, const char* b) {
	bool matching = true;
	int idx = 0;
	while (matching && a[idx] != 0x00 && b[idx] != 0x00) {
		//printf("a: %c al: %c, b: %c\n", a[idx], ToLowerCase(a[idx]), b[idx]);
		if (ToLowerCase(a[idx]) != b[idx]) {
			matching = false;
			break;
		}
		else if (b[idx] == 0x00) {
			matching = true;
			break;
			//the two strings differ, but the difference is b ended whil a continues, therefore a starts with b
		}
		idx++;
	}
	return matching;
}

bool containsChar(const char* str, const char* test) {
	int sIdx = 0;
	while (str[sIdx] != 0x00) {
		uint8_t tIdx = 0;
		while (test[tIdx] != 0x00 && test[tIdx] == str[sIdx + tIdx])
		{
			tIdx++;
		}
		if (test[tIdx] == 0x00) {
			//if I reached the end of the test sting while the consition that test and str must match, test is contained in match
			return true;
		}
		sIdx++;
	}
	return false;
}

int ProcessRepoNameForDisplayOrOriginChangeOutputWillBeMalloced(const char* repoToTest, char** output) {
	//input: repoToTest, the path of a repo. if it is one of the defined repos, return that, if it's not, produce the short notation
	//basically this should produce the displayed name for the repo in the output buffer and additionally indicate if it's a known one
	int matchindex = -1;
	for (int i = 0;i < 4;i++) {
		if (StartsWith(repoToTest, repocombos[1][i])) {
			matchindex = i;
			asprintf(output, "%s", repocombos[0][i]);
			break;
		}
	}
	if (matchindex == -1) {

		//value was not found in the defined ones -> make new one

		//if regex (?<proto>\w+?)://(?:www.)?(?<username>\w+@)?(?<host>[0-9a-zA-Z\.\-_]+):?(?<port>:\d*)?(?:/\S+)*/(?<repo>\S+) matches -> remote repo
		//then print ${repo} from ${proto}:${username}${host}${port}

		if (regexec(&RemoteRepoRegex, repoToTest, maxGroups, CapturedResults, 0) == 0)
		{
			//remote repo
			int grp1len = CapturedResults[1].rm_eo - CapturedResults[1].rm_so;
			int grp5len = CapturedResults[5].rm_eo - CapturedResults[5].rm_so;
			int grp1so = CapturedResults[1].rm_so;
			int grp5so = CapturedResults[5].rm_so;
			int len = grp1len + grp5len + 1 + 1;//proto+host+:+\0
			// printf("1: %i 5: %i len: %i\n", grp1len, grp5len, len);
			// fflush(stdout);
			(*output) = (char*)malloc(sizeof(char) * len);
			(*output)[0] = 0x00;
			strncpy((*output), repoToTest + grp1so, grp1len);
			(*output)[grp1len] = ':';
			strncpy((*output) + grp1len + 1, repoToTest + grp5so, grp5len);
			(*output)[len - 1] = 0x00;
		}
		else {
			//local repo
			asprintf(output, "%s", repoToTest);


			//in the .sh implementation I had used realpath relative to pwd to "shorten" the path, but I think it'd be better if I properly regex this up or something.
			//like /folder/folder/[...]/folder/NAME or /folder/[...]/NAME, though if the path is short enough, I'd like the full path
			//local repos $(realpath -q -s --relative-to="argv[2]" "$( echo "$fulltextremote" | grep -v ".\+://")" "")

			//for locals: realpath -q -s --relative-to="argv[2]" "Input"
		}
	}
	return matchindex;
}

char* FixImplicitProtocol(const char* input) {
	char* reply;
	if (containsChar(input, "@") && !containsChar(input, "://")) {
		//repo contains @ but not :// ie it is a ssh://, but ssh:// is implicit
		asprintf(&reply, "ssh://%s", input);
	}
	else {
		asprintf(&reply, "%s", input);
	}
	return reply;
}

int main(int argc, char** argv) {
	const char* RemoteRepoRegexString = "([a-zA-Z_][a-zA-Z0-9_\\-]*?)://(www.)?(([a-zA-Z_][a-zA-Z0-9_\\-]*)@)?([0-9a-zA-Z\\.\\-_]+):?(:([0-9]*))?(/[^ ]+)*/([^ ]+)"; // PCRE regex, but with escaped backslash
	/*
	Group 0: [ 0-51]: ssh://git@someprivateurl.de:2121/data/repos/ShellTools
	Group 1: [ 0- 3]: ssh
	Group 2: [4294967295-4294967295]:
	Group 3: [ 6-10]: git@
	Group 4: [ 6- 9]: git
	Group 5: [10-27]: someprivateurl.de
	Group 6: [27-32]: :1234
	Group 7: [28-32]: 1234
	Group 8: [32-43]: /data/repos
	Group 9: [44-51]: ShellTools
	 therefore I want ssh:someprivateurl.de -> 1:5
	*/

	int RegexReturnCode = regcomp(&RemoteRepoRegex, RemoteRepoRegexString, REG_EXTENDED | REG_NEWLINE);
	if (RegexReturnCode)
	{
		char* regErrorBuf = (char*)malloc(sizeof(char) * 1024);
		int elen = regerror(RegexReturnCode, &RemoteRepoRegex, regErrorBuf, 1024);
		printf("Could not compile regular expression '%s'. [%i(%s) {len:%i}]\n", RemoteRepoRegexString, RegexReturnCode, regErrorBuf, elen);
		free(regErrorBuf);
		return 1;
	};

	if (argc == 2) {
		char* Input = FixImplicitProtocol(argv[1]);//git@192.168.0.220 -> ssh://git@192.168.0.220
		char* foldername = Input;
		int iTemp = 0;
		while (Input[iTemp] != 0x00) {
			if (Input[iTemp] == '/') {
				foldername = Input + iTemp + 1;
			}
			iTemp++;
		}
		char* finalreponame;
		int idx = ProcessRepoNameForDisplayOrOriginChangeOutputWillBeMalloced(Input, &finalreponame);

		printf("%%F{009}%s %%F{001}from %%F{009}%s %%f", foldername, finalreponame);

		free(finalreponame);
		free(Input);
		return 0;
	}
	else if (argc == 3 || argc == 4) {
		//NOTE: DO NOT ALLOW CHANGES HERE IF NOT ON AN UTF-8 SYSTEM!!!
		char* hline = "─";
		char* vline = "│";
		char* vcross = "├";
		char* knee = "└";
		printf("%s (%i), %s (%i), %s (%i), %s (%i)", vline, (int)vline, hline, (int)hline, vcross, (int)vcross, knee, (int)knee);
		//~/repotools.elf /mnt/e/CODE SHOW
		//~/repotools.elf /mnt/e/CODE SET global/local/internal/none
		//I want to traverse the directory tree to find all submodules (know what origin, what branch) but ALSO find non-submodule repos (these shouldn't be changed)
		//read-in stuff from stdin (the submodules)

		/*
		in toplevel:
			-print git status (marker if not a sumbodule)
			-find submodules (non recursive)
			-find subdirectories
			-foreach subdirectory:
				-check if it's a git repo -> check if it contains a file/folder .git
					-check if it's in the submodule list
					-if change requested
						-check if current origin compatible
							-replace origin
				-do indentation
				recursive call to this folder to process
		*/

		if (argc == 4) {
			//change
		}
		//print (either because print itself was called or as status)
	}
	else {
		fprintf(stderr, "INVALID NUMBER OF PARAMETERS: %i, expected 1 (Repo for Prompt), 2 (print tree) or 3 (update source)\n", argc - 1);
		return 1;
	}
}

void ProcessDirectory(const char* directory, int recursiondepth) {

}
