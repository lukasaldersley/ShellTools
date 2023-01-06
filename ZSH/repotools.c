#/*
outname="$(basename $0 .c).elf"
echo "compiling $0 into ~/$outname"
gcc -std=c2x "$0" -o ~/"$outname"
exit
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <dirent.h>

bool Compare(const char* a, const char* b);
char ToLowerCase(char c);
char ToUpperCase(char c);
bool StartsWith(const char* a, const char* b);
bool containsChar(const char* str, const char* test);

#define COLOUR_Black	"\e[30m"
#define COLOUR_Red	"\e[31m"
#define COLOUR_Green	"\e[32m"
#define COLOUR_Yellow	"\e[33m"
#define COLOUR_Blue	"\e[34m"
#define COLOUR_Magenta	"\e[35m"
#define COLOUR_Cyan	"\e[36m"
#define COLOUR_White	"\e[37m"
#define COLOUR_BrightBlack	"\e[90m"
#define COLOUR_BrightRed	"\e[91m"
#define COLOUR_BrightGreen	"\e[92m"
#define COLOUR_BrightYellow	"\e[93m"
#define COLOUR_BrightBlue	"\e[94m"
#define COLOUR_BrightMagenta	"\e[95m"
#define COLOUR_BrightCyan	"\e[96m"
#define COLOUR_BrightWhite	"\e[97m"
#define COLOUR_CLEAR "\e[0m"

#define maxGroups 10
#define MaxLocations 10
const char* NAMES[10];
const char* LOCS[10];
uint8_t numLOCS = 0;
char* buf;
int bufCurLen;
const char* DEFAULT_NAME_NONE = "NONE";
const char* DEFAULT_PATH_NONE = "/dev/null";
const char* DEFAULT_NAME_GLOBAL = "GLOBAL";
const char* DEFAULT_PATH_GLOBAL = "ssh://git@someprivateurl.de:1234/data/repos";
regex_t RemoteRepoRegex;
regmatch_t CapturedResults[maxGroups];

typedef struct RepoInfo_t RepoInfo;
typedef struct RepoList_t RepoList;

struct RepoList_t {
	RepoList* prev;
	RepoList* next;
	RepoInfo* self;
};

struct RepoInfo_t {
	RepoInfo* parent;
	RepoList* children;
	int numChildren;
	char* name;
	char* path;
	char* branch;
	char* repoName;
	bool isGit;
	bool isSubModule;
	bool isBare;
	char* displayOrigin;
	char* parentRepo;
	int repoOrigin;
	int prevRepoOrigin;
};

void AddChild(RepoInfo* parent, RepoInfo* child) {
	if (parent->children == NULL) {
		parent->children = (RepoList*)malloc(sizeof(RepoList));
		parent->children->self = child;
		parent->children->next = NULL;
		parent->children->prev = NULL;
	}
	else {
		RepoList* current = parent->children;
		while (current->next != NULL)
		{
			current = current->next;
		}
		current->next = (RepoList*)malloc(sizeof(RepoList));
		current->next->prev = current;
		current->next->self = child;
		current->next->next = NULL;
	}
	child->parent = parent;
	parent->numChildren++;
}

char* FixImplicitProtocol(const char* input)
{
	char* reply;
	if (containsChar(input, "@") && !containsChar(input, "://"))
	{
		// repo contains @ but not :// ie it is a ssh://, but ssh:// is implicit
		asprintf(&reply, "ssh://%s", input);
	}
	else
	{
		asprintf(&reply, "%s", input);
	}
	return reply;
}

int ProcessRepoOriginForDisplayOutputWillBeMalloced(const char*, char**);

int ProcessRepo(RepoInfo* ri, int desiredorigin) {
	//printf("Processing %s\n", ri->path);
	char* processStr;
	char inputbuffer[1024];
	//IF 'git rev-parse --show-superproject-working-tree' outputs NOTHING, a repo is standalone, if there is output it will point to the parent repo
		//git rev-parse --is-bare-repository returns 'true'/'false' if in a bare repo
		//git rev-parse --abbrev-ref HEAD shows the current branch
	if (asprintf(&processStr, "git -C %s ls-remote --get-url origin", ri->path)) {
		FILE* fp = popen(processStr, "r");
		if (fp == NULL)
		{
			fprintf(stderr, "read origin failed");
		}
		// bool hasFullName = false;
		if (fp != NULL)
		{
			if (fgets(inputbuffer, 1023, fp) != NULL)
			{
				char* ib = FixImplicitProtocol(inputbuffer);
				char* tempptr = ib;
				for (int i = 0;i < 1024;i++) {
					if (ib[i] == '\n') {
						ib[i] = 0x00;
						break;
					}
					if (ib[i] == '/') {
						tempptr = ib + i + 1;
					}
				}
				//collect all data for git repos
				ri->repoOrigin = ProcessRepoOriginForDisplayOutputWillBeMalloced(ib, &ri->displayOrigin);
				asprintf(&ri->repoName, "%s", tempptr);
				tempptr = NULL;
				if (desiredorigin != -1 && ri->repoOrigin != -1 && ri->repoOrigin != desiredorigin) {
					//change
					ri->prevRepoOrigin = ri->repoOrigin;
					ri->repoOrigin = desiredorigin;
					char* changeCmd;
					asprintf(&changeCmd, "git -C %s remote set-url origin %s/%s", ri->path, LOCS[ri->repoOrigin], ri->repoName);
					printf("%s\n", changeCmd);
					FILE* fp2 = popen(changeCmd, "r");
					if (fp2 == NULL)
					{
						fprintf(stderr, "change origin failed");
					}
					pclose(fp2);
					free(changeCmd);
				}
				free(ib);
			}
		}
		pclose(fp);
		fp = NULL;
	}
	else {
		fprintf(stderr, "outofMem\n");
	}

	if (asprintf(&processStr, "git -C %s rev-parse --show-superproject-working-tree", ri->path)) {
		FILE* fp = popen(processStr, "r");
		if (fp == NULL)
		{
			fprintf(stderr, "read origin failed");
		}
		// bool hasFullName = false;
		if (fp != NULL)
		{
			if (fgets(inputbuffer, 1023, fp) != NULL)
			{
				for (int i = 0;i < 1024;i++) {
					if (inputbuffer[i] == '\n') {
						inputbuffer[i] = 0x00;
						break;
					}
				}
				asprintf(&ri->parentRepo, "%s", inputbuffer);
				//printf("strlen of parent (%s) of %s: %i\n", ri->parentRepo, ri->path, strlen(ri->parentRepo));
				ri->isSubModule = true;
			}
		}
		pclose(fp);
		fp = NULL;
	}
	else {
		fprintf(stderr, "outofMem\n");
	}

	if (asprintf(&processStr, "git -C %s rev-parse --is-bare-repository", ri->path)) {
		FILE* fp = popen(processStr, "r");
		if (fp == NULL)
		{
			fprintf(stderr, "read origin failed");
		}
		// bool hasFullName = false;
		if (fp != NULL)
		{
			if (fgets(inputbuffer, 1023, fp) != NULL)
			{
				for (int i = 0;i < 1024;i++) {
					if (inputbuffer[i] == '\n') {
						inputbuffer[i] = 0x00;
						break;
					}
				}
				//collect all data for git repos
				//inputbuffer now has true/false if the repo is bare
				ri->isBare = Compare(inputbuffer, "true");
			}
		}
		pclose(fp);
		fp = NULL;
	}
	else {
		fprintf(stderr, "outofMem\n");
	}

	if (asprintf(&processStr, "git -C %s rev-parse --abbrev-ref HEAD", ri->path)) {
		FILE* fp = popen(processStr, "r");
		if (fp == NULL)
		{
			fprintf(stderr, "read origin failed");
		}
		// bool hasFullName = false;
		if (fp != NULL)
		{
			if (fgets(inputbuffer, 1023, fp) != NULL)
			{
				for (int i = 0;i < 1024;i++) {
					if (inputbuffer[i] == '\n') {
						inputbuffer[i] = 0x00;
						break;
					}
				}
				//collect all data for git repos
				asprintf(&ri->branch, "%s", inputbuffer);
			}
		}
		pclose(fp);
		fp = NULL;
	}
	else {
		fprintf(stderr, "outofMem\n");
	}
}

RepoInfo* CreateDirStruct(const char* directoryPath, const char* directoryName, int newRepoSpec) {
	RepoInfo* ri = (RepoInfo*)malloc(sizeof(RepoInfo));
	ri->branch = NULL;
	ri->children = NULL;
	ri->parent = NULL;
	ri->parentRepo = NULL;
	ri->isGit = false;
	ri->isSubModule = false;
	ri->numChildren = 0;
	ri->prevRepoOrigin = -1;
	ri->prevRepoOrigin = -1;
	ri->displayOrigin = NULL;
	ri->isBare = false;
	ri->repoName = NULL;
	asprintf(&((*ri).name), "%s", directoryName);
	asprintf(&((*ri).path), "%s/%s", directoryPath, directoryName);
	DIR* directoryPointer;
	struct dirent* direntptr;
	directoryPointer = opendir(ri->path);
	if (directoryPointer != NULL)
	{
		while (direntptr = readdir(directoryPointer))
		{
			//printf("testing: %s (directory: %s)\n", direntptr->d_name, (direntptr->d_type == DT_DIR ? "TRUE" : "NO"));
			if (Compare(direntptr->d_name, ".git")) {
				ri->isGit = true;
				ProcessRepo(ri, newRepoSpec);
				continue;
			}
			else if (Compare(direntptr->d_name, ".") || Compare(direntptr->d_name, "..")) {
				continue;
			}
			else if (direntptr->d_type == DT_DIR) {
				AddChild(ri, CreateDirStruct(ri->path, direntptr->d_name, newRepoSpec));
			}
		}
		closedir(directoryPointer);
	}
	else
	{
		printf("failed on directory: %s\n", directoryName);
		perror("Couldn't open the directory");
	}
	return ri;
}

void printTree_internal(RepoInfo* ri, const char* parentPrefix, bool anotherSameLevelEntryFollows) {
	if (ri == NULL) {
		return;
	}
	if (ri->parent != NULL) {
		printf("%s%s── ", parentPrefix, anotherSameLevelEntryFollows ? "├" : "└");
	}
	if (ri->isGit) {
		printf("%s [%s" COLOUR_BrightGreen "GIT", ri->name, ri->isBare ? "BARE " : "");
		if (ri->isSubModule) {
			printf("-SM" COLOUR_CLEAR "@" COLOUR_BrightCyan "%s", ri->parentRepo);
		}
		printf(COLOUR_CLEAR "] %s@%s " COLOUR_Red "from ", ri->branch, ri->repoName);
		if (ri->prevRepoOrigin != -1) {
			printf(COLOUR_BrightRed "[%s(%i) -> %s(%i)]" COLOUR_CLEAR, NAMES[ri->prevRepoOrigin], ri->prevRepoOrigin, NAMES[ri->repoOrigin], ri->repoOrigin);
		}
		else {
			printf("%s%s%s", COLOUR_BrightRed, ri->displayOrigin, COLOUR_CLEAR);
		}
		printf("\n");
	}
	else {
		printf("%s\n", ri->name);
	}
	fflush(stdout);
	RepoList* current = ri->children;
	int procedSubDirs = 0;
	char* temp;
	asprintf(&temp, "%s%s", parentPrefix, (ri->parent != NULL) ? (anotherSameLevelEntryFollows ? "│   " : "    ") : "");
	while (current != NULL)
	{
		procedSubDirs++;
		printTree_internal(current->self, temp, procedSubDirs < ri->numChildren);
		current = current->next;
	}
	free(temp);
}

void printTree(RepoInfo* ri) {
	printTree_internal(ri, "", ri->numChildren > 1);
}

bool pruneTreeForGit(RepoInfo* ri) {
	bool hasGitInTree = false;
	if (ri->children != NULL) {
		RepoList* current = ri->children;
		RepoList* next = NULL;
		while (current != NULL)
		{
			next = current->next;
			if (pruneTreeForGit(current->self)) {
				hasGitInTree = true;
			}
			else {
				if (current->prev != NULL) {
					current->prev->next = current->next;
				}
				else {
					//it was the first->ri pointed to this initially
					ri->children = current->next;
				}
				if (current->next != NULL) {
					current->next->prev = current->prev;
				}
				ri->numChildren--;
				//printf("removing %s\n", current->self->name);
				free(current);
				current = NULL;
			}
			current = next;
		}
	}
	return ri->isGit || hasGitInTree;
}

void DoSetup() {
	NAMES[0] = DEFAULT_NAME_NONE;
	LOCS[0] = DEFAULT_PATH_NONE;
	NAMES[1] = DEFAULT_NAME_GLOBAL;
	LOCS[1] = DEFAULT_PATH_GLOBAL;
	numLOCS = 2;
	errno = 0;

	char* file;
	char* fileName = "/repoconfig.cfg";
	file = (char*)malloc(strlen(getenv("HOME")) + strlen(fileName) + 1); // to account for NULL terminator
	strcpy(file, getenv("HOME"));
	strcat(file, fileName);
	FILE* fp = fopen(file, "r");//open for read, will fail if file doesn't exist
	if (fp == NULL) {
		printf("config file didn't exist (%i: %s)\n", errno, strerror(errno));
		errno = 0;
		fp = fopen(file, "w+");//open for read/write -> create if not exists, then fill defaults, then read
		if (fp == NULL) {
			printf("couldn't create file (%i: %s)\n", errno, strerror(errno));
		}
		else {
			fprintf(fp, "LOCAL	ssh://127.0.0.1/data/repos\n");
			fprintf(fp, "INTERNAL	ssh://git@192.168.0.123/data/repos\n");
			fflush(fp);
			rewind(fp);
			printf("created default config file: %s\n", file);
		}
	}

	while (bufCurLen < 1024 - 2) {
		fgets(buf + bufCurLen, 1024 - bufCurLen - 1, fp);
		NAMES[numLOCS] = buf + bufCurLen;
		int linelen = 0;
		while (linelen < 1024) {
			if (buf[bufCurLen + linelen] == '\t') {
				//found a tab -> split sting by inserting 0x00 and save new start location
				buf[bufCurLen + linelen] = 0x00;
				linelen++;
				LOCS[numLOCS++] = buf + bufCurLen + linelen;
			}
			if (buf[bufCurLen + linelen] == '\n' || buf[bufCurLen + linelen] == 0x00) {
				//found EOL -> break
				buf[bufCurLen + linelen] = 0x00;
				linelen++;
				break;
			}
			linelen++;
		}
		bufCurLen += linelen;
		if (linelen == 1) {//only the advance I make when encountering \n or 0x00 -> empty line -> done
			break;
		}
	}
	free(file);
}

int ProcessRepoOriginForDisplayOutputWillBeMalloced(const char* repoToTest, char** output)
{
	// input: repoToTest, the path of a repo. if it is one of the defined repos, return that, if it's not, produce the short notation
	// basically this should produce the displayed name for the repo in the output buffer and additionally indicate if it's a known one
	int matchindex = -1;
	for (int i = 0; i < numLOCS; i++)
	{
		if (StartsWith(repoToTest, LOCS[i]))
		{
			matchindex = i;
			asprintf(output, "%s", NAMES[i]);
			break;
		}
	}
	if (matchindex == -1)
	{

		// value was not found in the defined ones -> make new one

		// if regex (?<proto>\w+?)://(?:www.)?(?<username>\w+@)?(?<host>[0-9a-zA-Z\.\-_]+):?(?<port>:\d*)?(?:/\S+)*/(?<repo>\S+) matches -> remote repo
		// then print ${repo} from ${proto}:${username}${host}${port}

		if (regexec(&RemoteRepoRegex, repoToTest, maxGroups, CapturedResults, 0) == 0)
		{
			// remote repo
			int grp1len = CapturedResults[1].rm_eo - CapturedResults[1].rm_so;
			int grp5len = CapturedResults[5].rm_eo - CapturedResults[5].rm_so;
			int grp1so = CapturedResults[1].rm_so;
			int grp5so = CapturedResults[5].rm_so;
			int len = grp1len + grp5len + 1 + 1; // proto+host+:+\0
			// printf("1: %i 5: %i len: %i\n", grp1len, grp5len, len);
			// fflush(stdout);
			(*output) = (char*)malloc(sizeof(char) * len);
			(*output)[0] = 0x00;
			strncpy((*output), repoToTest + grp1so, grp1len);
			(*output)[grp1len] = ':';
			strncpy((*output) + grp1len + 1, repoToTest + grp5so, grp5len);
			(*output)[len - 1] = 0x00;
		}
		else
		{
			// local repo
			asprintf(output, "%s", repoToTest);

			// in the .sh implementation I had used realpath relative to pwd to "shorten" the path, but I think it'd be better if I properly regex this up or something.
			// like /folder/folder/[...]/folder/NAME or /folder/[...]/NAME, though if the path is short enough, I'd like the full path
			// local repos $(realpath -q -s --relative-to="argv[2]" "$( echo "$fulltextremote" | grep -v ".\+://")" "")

			// for locals: realpath -q -s --relative-to="argv[2]" "Input"
		}
	}
	return matchindex;
}

int main(int argc, char** argv)
{
	if (argc < 2 || argc>4) {
		printf("Invalid number of parameters: %i (vaild are 1 [Repo for Prompt], 2 [print tree] or 3 [print tree+update source])\n", argc - 1);
		return -1;
	}
	buf = (char*)malloc(sizeof(char) * 1024);
	bufCurLen = 0;
	if (buf == NULL) {
		printf("couldn't malloc required buffer");
	}
	DoSetup();
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

	if (argc == 2) //show origin info for command prompt
	{
		if (Compare(argv[1], "origin")) {
			//if the input is just 'origin' I assume $(git ls-remote --get-url origin) just re-output "origin because it was called in a non-git folder -> skip this and print nothing"
			return 0;
		}

		char* Input = FixImplicitProtocol(argv[1]); // git@192.168.0.123 -> ssh://git@192.168.0.123

		//the following while construct iterates over the input and advances a pointer into the string as to create effectively a second string in the same memoryspace containing only what was AFTER the LAST / -> that'll be the repo name, (and usually unless explicitly changed during cloning it'll also be the top-level folder name)
		char* foldername = Input;
		int iTemp = 0;
		while (Input[iTemp] != 0x00)
		{
			if (Input[iTemp] == '/')
			{
				foldername = Input + iTemp + 1;
			}
			iTemp++;
		}

		char* finalreponame;
		int idx = ProcessRepoOriginForDisplayOutputWillBeMalloced(Input, &finalreponame);

		//possibly make an addition to have reponame@foldername if they aren't the same (if cou cloned a repo into a non-default folder, eg git clone /dev/null/TestRepo ButIWantThisFolderForTestRepo)
		//though to do that I'd need to figure out what the toplevel folder acutally is: i can't use pwd since I could be deep inside subfolders inside the repo
		printf("%%F{009}%s %%F{001}from %%F{009}%s %%f", foldername, finalreponame);

		free(finalreponame);
		free(Input);
		return 0;
	}
	else//print Structure
	{
		//Possible calls:
		//~/repotools.elf /mnt/e/ShellTools SHOW
		//~/repotools.elf /mnt/e/ShellTools SET global|local|internal|none
		//~/repotools.elf LIST ORIGINS


		int requestedNewOrigin = -1;
		if (argc == 4 && Compare(argv[2], "SET")) {//a change was requested
			for (int i = 0;i < numLOCS;i++) {
				if (Compare(argv[3], NAMES[i])) {//found requested new origin
					requestedNewOrigin = i;
					break;
				}
			}
			if (requestedNewOrigin == -1) {
				printf("new origin specification " COLOUR_BrightRed "'%s'" COLOUR_CLEAR " is unknown. available origin specifications are:\n", argv[3]);
				requestedNewOrigin = -2;//this is a special flag to drop into LIST ORIGINS and exit (as error handling mechanism)
			}
		}

		if (requestedNewOrigin == -2 || (Compare(argv[1], "LIST") && Compare(argv[2], "ORIGINS"))) {
			for (int i = 0;i < numLOCS;i++) {
				printf(COLOUR_Red "%s" COLOUR_CLEAR " (-> %s)\n", NAMES[i], LOCS[i]);
			}
			return 0;
		}
		/*
		specifically the repo name, branch and remote (shortened, maybe also longversion in addition)
		of particular interest are submodules. there I want to also mark what repo they are a submodule of.
				On top of that I want to add a parameter denoting the new target origin.
		If this parameter is set all repos/submodules with a known remote (ie my repos) whould be bent to the new remote
		this should also be marked in the printout of that run.
		*/

		/*
		I can get the list of all submodules by using
		git -C *TARGET_FOLDER* submodule status --recursive | sed -n 's|.[0-9a-fA-F]\+ \([^ ]\+\)\( .*\)\?|\1|p'
		This would give me a list of submodules of the current repo. I could then compare the output to when I come across them. This method provides me with a list of relative paths
		however I could also run
		git -C *TARGET_FOLDER* rev-parse --show-superproject-working-tree
		that would give me the path of the parent repo (in my experiments it always was an absolute path) and return nothing in a non-submodule repo

		I have decided to use the second option (rev-parse). I will post-process the output and parse it's name out or use realpath relative to to get the ../../../../something notation
		*/

		//IF 'git rev-parse --show-superproject-working-tree' outputs NOTHING, a repo is standalone, if there is output it will point to the parent repo
		//git rev-parse --is-bare-repository returns 'true'/'false' if in a bare repo
		//git rev-parse --abbrev-ref HEAD shows the current branch


		RepoInfo* treeroot = CreateDirStruct("", argv[1], requestedNewOrigin);
		pruneTreeForGit(treeroot);
		printTree(treeroot);
	}
	free(buf);
}



//-------------------------------------------------------------------------------------------------------------------------------------------
//as of here there's just unspecified helper functions
//-------------------------------------------------------------------------------------------------------------------------------------------

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
	if (c >= 'a')
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
		// printf("a: %c al: %c, b: %c\n", a[idx], ToLowerCase(a[idx]), b[idx]);
		if (ToLowerCase(a[idx]) != b[idx])
		{
			matching = false;
			break;
		}
		else if (b[idx] == 0x00)
		{
			matching = true;
			break;
			// the two strings differ, but the difference is b ended whil a continues, therefore a starts with b
		}
		idx++;
	}
	return matching;
}

bool containsChar(const char* str, const char* test)
{
	int sIdx = 0;
	while (str[sIdx] != 0x00)
	{
		uint8_t tIdx = 0;
		while (test[tIdx] != 0x00 && test[tIdx] == str[sIdx + tIdx])
		{
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
