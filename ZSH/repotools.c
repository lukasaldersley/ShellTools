#/*
outname="$(basename $0 .c).elf"
printf "compiling $0 into ~/$outname"
gcc -std=c2x -Wall "$0" -o ~/"$outname"
printf " -> \e[32mDONE\e[0m($?)\n"
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

bool ContainsString(const char* str, const char* test)
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

#define COLOUR_GIT_BARE "\e[38;5;006m"

//BrightGreen	"\e[92m"
#define COLOUR_GIT_INDICATOR "\e[38;5;002m"

//BrightCyan	"\e[96m"
#define COLOUR_GIT_PARENT "\e[38;5;004m"

//BrightRed	"\e[91m"
#define COLOUR_GIT_NAME "\e[38;5;009m"

//COLOUR_Yellow	"\e[33m"
#define COLOUR_GIT_BRANCH "\e[38;5;001m"

//Magenta	"\e[35m"
#define COLOUR_GIT_ORIGIN "\e[38;5;005m"

#define COLOUR_GIT_COMMITS "\e[38;5;208m"
#define COLOUR_GIT_MERGES "\e[38;5;009m"
#define COLOUR_GIT_STAGED "\e[38;5;010m"
#define COLOUR_GIT_MODIFIED "\e[38;5;226m"
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
	RepoInfo* ParentDirectory;
	RepoList* SubDirectories;
	char* DirectoryName;
	char* DirectoryPath;//path+name
	char* RepositoryName;//the name of the repo itself
	char* RepositoryDisplayedOrigin;
	char* RepositoryUnprocessedOrigin;
	char* branch;
	char* parentRepo;
	int SubDirectoryCount;
	int RepositoryOriginID;
	int RepositoryOriginID_PREVIOUS;
	bool isGit;
	bool isSubModule;
	bool isBare;
	bool DirtyWorktree;
	int RemoteCommits;
	int LocalCommits;
	int UntrackedFiles;
	int ModifiedFiles;
	int StagedChanges;
	int ActiveMergeFiles;
	int stashes;
};

RepoInfo* AllocRepoInfo(const char* directoryPath, const char* directoryName) {
	RepoInfo* ri = (RepoInfo*)malloc(sizeof(RepoInfo));
	ri->ParentDirectory = NULL;
	ri->SubDirectories = NULL;
	ri->DirectoryName = NULL;
	ri->DirectoryPath = NULL;
	ri->RepositoryName = NULL;
	ri->RepositoryDisplayedOrigin = NULL;
	ri->RepositoryUnprocessedOrigin = NULL;
	ri->branch = NULL;
	ri->parentRepo = NULL;
	ri->SubDirectoryCount = 0;
	ri->RepositoryOriginID = -1;
	ri->RepositoryOriginID_PREVIOUS = -1;
	ri->isGit = false;
	ri->isSubModule = false;
	ri->isBare = false;
	ri->DirtyWorktree = false;
	ri->RemoteCommits = 0;
	ri->LocalCommits = 0;
	ri->UntrackedFiles = 0;
	ri->ModifiedFiles = 0;
	ri->StagedChanges = 0;
	ri->ActiveMergeFiles = 0;
	ri->stashes = 0;

	asprintf(&(ri->DirectoryName), "%s", directoryName);
	asprintf(&(ri->DirectoryPath), "%s%s%s", directoryPath, ((strlen(directoryPath) > 0) ? "/" : ""), directoryName);
	//printf("working on %s\n", ri->DirectoryPath);
	return ri;
}

void DeallocRepoInfoStrings(RepoInfo* ri) {
	if (ri->DirectoryName != NULL) { free(ri->DirectoryName); }
	if (ri->DirectoryPath != NULL) { free(ri->DirectoryPath); }
	if (ri->RepositoryName != NULL) { free(ri->RepositoryName); }
	if (ri->RepositoryDisplayedOrigin != NULL) { free(ri->RepositoryDisplayedOrigin); }
	if (ri->branch != NULL) { free(ri->branch); }
	if (ri->parentRepo != NULL) { free(ri->parentRepo); }
}

void AllocUnsetStringsToEmpty(RepoInfo* ri) {
	if (ri->DirectoryName == NULL) { ri->DirectoryName = (char*)malloc(sizeof(char));ri->DirectoryName[0] = 0x00; }
	if (ri->DirectoryPath == NULL) { ri->DirectoryPath = (char*)malloc(sizeof(char));ri->DirectoryPath[0] = 0x00; }
	if (ri->RepositoryName == NULL) { ri->RepositoryName = (char*)malloc(sizeof(char));ri->RepositoryName[0] = 0x00; }
	if (ri->RepositoryDisplayedOrigin == NULL) { ri->RepositoryDisplayedOrigin = (char*)malloc(sizeof(char));ri->RepositoryDisplayedOrigin[0] = 0x00; }
	if (ri->branch == NULL) { ri->branch = (char*)malloc(sizeof(char));ri->branch[0] = 0x00; }
	if (ri->parentRepo == NULL) { ri->parentRepo = (char*)malloc(sizeof(char));ri->parentRepo[0] = 0x00; }
}




char* ExecuteProcess(const char* args) {
	int size = 1024;
	char* result = malloc(sizeof(char) * size);
	if (result == NULL) {
		return NULL;
	}
	FILE* fp = popen(args, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "failed running process %s\n", args);
	}
	else {
		if (fgets(result, size - 1, fp) == NULL)
		{
			/*
			RETURN VALUE
				fgetc(), getc(), and getchar() return the character read as an unsigned char cast to an int or EOF on end of file or error.
				fgets() returns s on success, and NULL on error or when end of file occurs while no characters have been read.
			*/
			//show superprojet working tree returns 0 bytes if it's a toplevel thing -> just print back an empty string
			result[0] = 0x00;
		}
	}
	pclose(fp);
	return result;
}

void CheckExtendedGitStatus(RepoInfo* ri) {
	int size = 1024;
	char* result = malloc(sizeof(char) * size);
	if (result == NULL) {
		fprintf(stderr, "OUT OF MEMORY");
		return;//TODO indicate failure
	}
	char* command;
	asprintf(&command, "git -C %s status --porcelain=v2 -b", ri->DirectoryPath);
	FILE* fp = popen(command, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "failed running process %s\n", command);
	}
	else {
		while (fgets(result, size - 1, fp) != NULL)
		{
			if (result[0] == '1' || result[1] == '2') {//standard changes(add/modify/delete/...) and rename/copy
				if (result[2] != '.') {
					//staged change
					ri->StagedChanges++;
				}
				if (result[3] != '.') {
					//not staged change
					ri->ModifiedFiles++;
				}
			}
			else if (result[0] == '?') {
				//untracked file
				ri->UntrackedFiles++;
			}
			else if (result[0] == 'u') {//merge in progress
				ri->ActiveMergeFiles++;
			}
			else if (StartsWith(result, "# branch.ab ")) {
				//local/remote commits
				int offset = 13;
				int i = 0;
				while (result[offset + i] >= 0x30 && result[offset + i] <= 0x39) {
					i++;
				}
				result[offset + i] = 0x00;
				ri->LocalCommits = atoi(result + offset);
				ri->RemoteCommits = atoi(result + offset + i + 2);

			}
			else if (StartsWith(result, "# stash ")) {
				char* num = result + 8;
				ri->stashes = atoi(num);
			}
		}
	}
	free(result);
	pclose(fp);
	free(command);
}

void TerminateStrOnCR_LF_BEL(char* str) {
	int i = 0;
	while (str[i] != 0x00) {
		if (str[i] == '\n' || str[i] == '\r' || str[i] == '\a') {
			str[i] = 0x00;
			return;
		}
		i++;
	}
}

void AddChild(RepoInfo* parent, RepoInfo* child) {//OK
	//if the ParentDirectory node doesn't have any SubDirectories, it also won't have the list structure -> allocate and create it.
	if (parent->SubDirectories == NULL) {
		parent->SubDirectories = (RepoList*)malloc(sizeof(RepoList));
		parent->SubDirectories->self = child;
		parent->SubDirectories->next = NULL;
		parent->SubDirectories->prev = NULL;
	}
	//the ParentDirectory already has a child -> find the end of the list and insert it there
	else {
		RepoList* current = parent->SubDirectories;
		while (current->next != NULL)
		{
			current = current->next;
		}
		current->next = (RepoList*)malloc(sizeof(RepoList));
		current->next->prev = current;
		current->next->self = child;
		current->next->next = NULL;
	}
	child->ParentDirectory = parent;
	parent->SubDirectoryCount++;
}

char* FixImplicitProtocol(const char* input)
{
	char* reply;
	if (ContainsString(input, "@") && !ContainsString(input, "://"))
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

int cpyString(char* dest, const char* src, int maxCount) {
	for (int i = 0; i < maxCount; i++) {
		dest[i] = src[i];
		if (src[i] == 0x00) {
			return i;
		}
	}
	return maxCount;
}

/*
	I can get the list of all submodules by using
	git -C *TARGET_FOLDER* submodule status --recursive | sed -n 's|.[0-9a-fA-F]\+ \([^ ]\+\)\( .*\)\?|\1|p'
	This would give me a list of submodules of the current repo. I could then compare the output to when I come across them.
	This method provides me with a list of relative paths
	*/

bool TestPathForRepoAndParseIfExists(RepoInfo* ri, int desiredorigin, bool DoProcessWorktree, bool BeThorough) {
	//the return value is just: has an error occurred

	// I)   rev-parse --show-toplevel ; rev-parse --is-inside-git-dir ; rev-parse --is-bare-repository : $pwd ; false ; false => top level of worktree
	// II)  rev-parse --show-toplevel ; rev-parse --is-inside-git-dir ; rev-parse --is-bare-repository : anything other than $pwd ; false ; false => lower level of worktree
	// III) rev-parse --show-toplevel ; rev-parse --is-inside-git-dir ; rev-parse --is-bare-repository : error ; true ; false => inside .git of a worktree repo
	// IV)  rev-parse --show-toplevel ; rev-parse --is-inside-git-dir ; rev-parse --is-bare-repository : error ; true ; true => inside bare repo
	//basically I just need to model I and IV to determin if and what sort of git folder I'm dealing with

	char* cmd;
	if (BeThorough || DoProcessWorktree) {
		asprintf(&cmd, "git -C \"%s\" rev-parse --is-bare-repository 2>/dev/null", ri->DirectoryPath);
		char* bareRes = ExecuteProcess(cmd);
		free(cmd);
		if (bareRes == NULL) {
			//Encountered error -> quit
			DeallocRepoInfoStrings(ri);
			free(ri);
			return false;
		}
		TerminateStrOnCR_LF_BEL(bareRes);
		ri->isBare = Compare(bareRes, "true");
		free(bareRes);
		bareRes = NULL;
		ri->isGit = ri->isBare;//initial value

		if (!ri->isBare) {
			asprintf(&cmd, "git -C \"%s\" rev-parse --show-toplevel 2>/dev/null", ri->DirectoryPath);
			char* tlres = ExecuteProcess(cmd);
			TerminateStrOnCR_LF_BEL(tlres);
			free(cmd);
			ri->isGit = Compare(ri->DirectoryPath, tlres);
			//printf("dir %s tl: %s (%i) dpw: %i\n", ri->DirectoryPath, tlres, ri->isGit, DoProcessWorktree);
			free(tlres);
			if (DoProcessWorktree && !(ri->isGit)) {
				//usually we are done at this stage (only treat as git if in toplevel, but this is for the prompt substitution where it needs to work anywhere in git)
				asprintf(&cmd, "git -C \"%s\" rev-parse --is-inside-work-tree 2>/dev/null", ri->DirectoryPath);
				char* wtres = ExecuteProcess(cmd);
				TerminateStrOnCR_LF_BEL(wtres);
				free(cmd);
				ri->isGit = Compare(wtres, "true");
				free(wtres);
			}
		}
	}
	//if ri->isGit isn't set here it's not a repo -> I don't need to continue in this case
	if (!ri->isGit) {
		return true;
	}

	//as of here it's guaranteed that the current folder IS a git directory OF SOME TYPE (if it wasn't I would have exited above)

	//this tries to obtain the branch, or if that fails tag and if both fail the commit hash
	asprintf(&cmd, "git -C  \"%1$s\" symbolic-ref --short HEAD 2>/dev/null || git -C \"%1$s\" describe --tags --exact-match HEAD 2>/dev/null || git -C \"%1$s\" rev-parse --short HEAD", ri->DirectoryPath);
	ri->branch = ExecuteProcess(cmd);
	TerminateStrOnCR_LF_BEL(ri->branch);
	//printf("branch : %s\n", ri->branch);
	free(cmd);
	cmd = NULL;

	//if 'git rev-parse --show-superproject-working-tree' outputs NOTHING, a repo is standalone, if there is output it will point to the parent repo
	asprintf(&cmd, "git -C \"%s\" rev-parse --show-superproject-working-tree", ri->DirectoryPath);
	ri->parentRepo = ExecuteProcess(cmd);
	TerminateStrOnCR_LF_BEL(ri->parentRepo);
	ri->isSubModule = !((ri->parentRepo)[0] == 0x00);
	free(cmd);

	CheckExtendedGitStatus(ri);
	ri->DirtyWorktree = !(ri->ActiveMergeFiles == 0 && ri->ModifiedFiles == 0 && ri->StagedChanges == 0);

	asprintf(&cmd, "git -C \"%s\" ls-remote --get-url origin", ri->DirectoryPath);
	ri->RepositoryUnprocessedOrigin = ExecuteProcess(cmd);
	free(cmd);
	if (ri->RepositoryUnprocessedOrigin == NULL) {
		DeallocRepoInfoStrings(ri);
		free(ri);
		return false;
	}
	cmd = NULL;
	TerminateStrOnCR_LF_BEL(ri->RepositoryUnprocessedOrigin);
	if (Compare(ri->RepositoryUnprocessedOrigin, "origin")) {
		//if git ls-remote --get-url origin returns 'origin' it means either the folder is not a git repository OR it's a repository without remote (local only)
		//in this case since I already checked this IS a repo, it MUST be a repo without remote
		asprintf(&ri->RepositoryDisplayedOrigin, "NO_REMOTE");
		int i = 0;
		char* tempPtr = ri->DirectoryPath;
		while (ri->DirectoryPath[i] != 0x00) {
			if (ri->DirectoryPath[i] == '/') {
				tempPtr = ri->DirectoryPath + i + 1;
			}
			i++;
		}
		asprintf(&ri->RepositoryName, "%s", tempPtr);
		return true;
	}
	char* FixedProtoOrigin = FixImplicitProtocol(ri->RepositoryUnprocessedOrigin);

	//printf("fixedorigin: %s\n", FixedProtoOrigin);

	// input: repoToTest, the path of a repo. if it is one of the defined repos, return that, if it's not, produce the short notation
	// basically this should produce the displayed name for the repo in the output buffer and additionally indicate if it's a known one
	ri->RepositoryOriginID = -1;
	for (int i = 0; i < numLOCS; i++)
	{
		if (StartsWith(FixedProtoOrigin, LOCS[i]))
		{
			ri->RepositoryOriginID = i;
			asprintf(&ri->RepositoryDisplayedOrigin, "%s", NAMES[i]);
			break;
		}
	}


	// value was not found in the defined ones -> make new one

	char* sedCmd;
	asprintf(&sedCmd, "echo \"%s\" | sed -nE 's~^([a-zA-Z0-9_]+):\\/\\/(([a-zA-Z0-9_]+)@){0,1}([-0-9a-zA-Z_\\.]+)(\\:([0-9]+)){0,1}([\\:\\/]([-0-9a-zA-Z_]+)){0,1}.*/([-0-9a-zA-Z_]+)(\\.git\\/{0,1})?$~\\1|\\3|\\4|\\6|\\8|\\9~p'", FixedProtoOrigin);
	//printf("sedcmd <%s>", sedCmd);
	//fflush(stdout);
	char* sedRes = ExecuteProcess(sedCmd);
	TerminateStrOnCR_LF_BEL(sedRes);
	if (sedRes[0] == 0x00) {
		// local repo
		if (ri->RepositoryOriginID == -1)
		{
			asprintf(&ri->RepositoryDisplayedOrigin, "%s", FixedProtoOrigin);
		}
		char* tempptr = FixedProtoOrigin;
		char* walker = FixedProtoOrigin;
		while (*walker != 0x00) {
			if (*walker == '/') {
				tempptr = walker + 1;
			}
			walker++;
		}
		asprintf(&ri->RepositoryName, "%s", tempptr);

		// in the .sh implementation I had used realpath relative to pwd to "shorten" the path, but I think it'd be better if I properly regex this up or something.
		// like /folder/folder/[...]/folder/NAME or /folder/[...]/NAME, though if the path is short enough, I'd like the full path
		// local repos $(realpath -q -s --relative-to="argv[2]" "$( echo "$fulltextremote" | grep -v ".\+://")" "")

		// for locals: realpath -q -s --relative-to="argv[2]" "Input"
		//local repo
	}
	else {
		//printf("have sed response: <%s>\n", sedRes);
		//fflush(stdout);
		//parse out the elements from the input (proto|user|host|port|account|directory)
#define ptrcount 7
		char* ptrs[ptrcount];
		ptrs[0] = sedRes;
		ptrs[1] = sedRes;
		int cptr = 1;
		while (*ptrs[cptr] != 0x00 && cptr < (ptrcount - 1)) {
			if (*ptrs[cptr] == '|') {
				*ptrs[cptr] = 0x00;
				ptrs[cptr + 1] = ptrs[cptr]++;
				cptr++;
			}
			ptrs[cptr]++;
		}
		if (*ptrs[5] != 0x00) {//repo name
			asprintf(&ri->RepositoryName, "%s", ptrs[5]);
		}

		//for (int i = 0;i < ptrcount - 1;i++) {
		//	printf("element %i is %s\n", i, ptrs[i]);
		//}

		//proto:user_if_not_git@host:port:folder_if_gitHub
		if (ri->RepositoryOriginID == -1)
		{
			ri->RepositoryDisplayedOrigin = (char*)malloc(sizeof(char) * 256);
			int currlen = 0;
			currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[0], 255 - currlen);//proto
			currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ":", 255 - currlen);//:
			if (!Compare(ptrs[1], "git") && Compare(ptrs[0], "ssh")) {//if name is NOT git then print it but only print if it was ssh
				currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[1], 255 - currlen);//username
				currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, "@", 255 - currlen);//proto
			}
			currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[2], 255 - currlen);//host
			if (*ptrs[3] != 0x00) {//if port is given print it
				currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ":", 255 - currlen);//proto
				currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[3], 255 - currlen);//username
			}
			if (*ptrs[4] != 0x00 && (Compare(ptrs[2], "github.com") || Compare(ptrs[2], "gitlab.com") || Compare(ptrs[2], "cc-github.bmwgroup.net"))) {//host is github or gitlab and I can parse a github username also add it
				currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ":", 255 - currlen);//proto
				currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[4], 255 - currlen);//service username
			}
		}
#undef ptrcount
	}
	free(sedRes);

	//once I have the current and new repo origin IDs perform the change
	if (desiredorigin != -1 && ri->RepositoryOriginID != -1 && ri->RepositoryOriginID != desiredorigin) {

		//change
		ri->RepositoryOriginID_PREVIOUS = ri->RepositoryOriginID;
		ri->RepositoryOriginID = desiredorigin;
		char* changeCmd;
		asprintf(&changeCmd, "git -C \"%s\" remote set-url origin %s/%s", ri->DirectoryPath, LOCS[ri->RepositoryOriginID], ri->RepositoryName);
		printf("%s\n", changeCmd);
		ExecuteProcess(changeCmd);
		free(changeCmd);
	}


	return true;
}

RepoInfo* CreateDirStruct(const char* directoryPath, const char* directoryName, int newRepoSpec, bool BeThorough) {
	RepoInfo* ri = AllocRepoInfo(directoryPath, directoryName);
	if (BeThorough) {
		//If I am searching thorough I need to know if I encountered a bare repo, so I need to check the current folder first.
		//however I am quick searching I need to know if the current folder contains a file/folder named .git, therefor in that case I need to process the subfolders first
		//so this call is conditioned on BeThorough being true while at the end of this function I have basically the same call conditionend to BeThorough being false
		TestPathForRepoAndParseIfExists(ri, newRepoSpec, false, true);
	}

	DIR* directoryPointer;
	struct dirent* direntptr;
	directoryPointer = opendir(ri->DirectoryPath);
	if (directoryPointer != NULL)
	{
		while ((direntptr = readdir(directoryPointer)))
		{
			//printf("testing: %s (directory: %s)\n", direntptr->d_name, (direntptr->d_type == DT_DIR ? "TRUE" : "NO"));
			if (!BeThorough && Compare(direntptr->d_name, ".git")) {
				ri->isGit = true;
			}
			if (Compare(direntptr->d_name, ".") || Compare(direntptr->d_name, "..")) {
				continue;
			}
			else if (direntptr->d_type == DT_DIR && !ri->isBare) {
				AddChild(ri, CreateDirStruct(ri->DirectoryPath, direntptr->d_name, newRepoSpec, BeThorough));
			}
		}
		closedir(directoryPointer);
	}
	else
	{
		printf("failed on directory: %s\n", directoryName);
		perror("Couldn't open the directory");
	}
	if (!BeThorough) {
		//for an explanation to this call see the top of this function where another TestPathForRepoAndParseIfExists call exists
		TestPathForRepoAndParseIfExists(ri, newRepoSpec, false, false);
	}
	return ri;
}

char* ConstructGitStatusString(RepoInfo* ri) {
#define GIT_SEG_5_MAX_LEN 128
	int rbLen = 0;
	char* rb = (char*)malloc(sizeof(char) * GIT_SEG_5_MAX_LEN);
	rb[0] = 0x00;
	int temp;
	if (ri->LocalCommits > 0 || ri->RemoteCommits > 0 || ri->stashes > 0) {
		temp = snprintf(rb + rbLen, GIT_SEG_5_MAX_LEN - rbLen, " {" COLOUR_GIT_COMMITS "%i\u21e3 %i\u21e1" COLOUR_CLEAR, ri->RemoteCommits, ri->LocalCommits);
		if (temp < GIT_SEG_5_MAX_LEN && temp>0) {
			rbLen += temp;
		}
		if (ri->stashes > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_5_MAX_LEN - rbLen, " %i#", ri->stashes);
			if (temp < GIT_SEG_5_MAX_LEN && temp>0) {
				rbLen += temp;
			}
		}
		temp = snprintf(rb + rbLen, GIT_SEG_5_MAX_LEN - rbLen, "}");
		if (temp < GIT_SEG_5_MAX_LEN && temp>0) {
			rbLen += temp;
		}
	}

	if (ri->StagedChanges > 0 || ri->ModifiedFiles > 0 || ri->UntrackedFiles > 0) {
		temp = snprintf(rb + rbLen, GIT_SEG_5_MAX_LEN - rbLen, " <");
		if (temp < GIT_SEG_5_MAX_LEN && temp>0) {
			rbLen += temp;
		}

		if (ri->ActiveMergeFiles > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_5_MAX_LEN - rbLen, COLOUR_GIT_MERGES "%d!" COLOUR_CLEAR " ", ri->ActiveMergeFiles);
			if (temp < GIT_SEG_5_MAX_LEN && temp>0) {
				rbLen += temp;
			}
		}

		if (ri->StagedChanges > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_5_MAX_LEN - rbLen, COLOUR_GIT_STAGED "%d+" COLOUR_CLEAR " ", ri->StagedChanges);
			if (temp < GIT_SEG_5_MAX_LEN && temp>0) {
				rbLen += temp;
			}
		}

		if (ri->ModifiedFiles > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_5_MAX_LEN - rbLen, COLOUR_GIT_MODIFIED "%d*" COLOUR_CLEAR " ", ri->ModifiedFiles);
			if (temp < GIT_SEG_5_MAX_LEN && temp>0) {
				rbLen += temp;
			}
		}

		if (ri->UntrackedFiles > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_5_MAX_LEN - rbLen, "%d? ", ri->UntrackedFiles);
			if (temp < GIT_SEG_5_MAX_LEN && temp>0) {
				rbLen += temp;
			}
		}

		rb[--rbLen] = 0x00;//bin a space (after the file listings)

		temp = snprintf(rb + rbLen, GIT_SEG_5_MAX_LEN - rbLen, ">");
		if (temp < GIT_SEG_5_MAX_LEN && temp>0) {
			rbLen += temp;
		}
	}
	return rb;
}

void printTree_internal(RepoInfo* ri, const char* parentPrefix, bool anotherSameLevelEntryFollows) {
	if (ri == NULL) {
		return;
	}
	if (ri->ParentDirectory != NULL) {
		printf("%s%s\u2500\u2500 ", parentPrefix, anotherSameLevelEntryFollows ? "\u251c" : "\u2514");
	}
	if (ri->isGit) {
		printf("%s [" COLOUR_GIT_BARE "%s" COLOUR_GIT_INDICATOR "GIT", ri->DirectoryName, ri->isBare ? "BARE " : "");
		if (ri->isSubModule) {
			printf("-SM" COLOUR_CLEAR "@" COLOUR_GIT_PARENT "%s", ri->parentRepo);
		}
		printf(COLOUR_CLEAR "] " COLOUR_GIT_NAME "%s" COLOUR_CLEAR " on " COLOUR_GIT_BRANCH "%s " COLOUR_CLEAR "from ", ri->RepositoryName, ri->branch);
		if (ri->RepositoryOriginID_PREVIOUS != -1) {
			printf(COLOUR_GIT_ORIGIN "[%s(%i) -> %s(%i)]" COLOUR_CLEAR, NAMES[ri->RepositoryOriginID_PREVIOUS], ri->RepositoryOriginID_PREVIOUS, NAMES[ri->RepositoryOriginID], ri->RepositoryOriginID);
		}
		else {
			printf(COLOUR_GIT_ORIGIN "%s" COLOUR_CLEAR, ri->RepositoryDisplayedOrigin);
		}

		//if (ri->DirtyWorktree) {
		//	printf(COLOUR_BrightYellow " \u2573" COLOUR_CLEAR);
		//}
		char* temp = ConstructGitStatusString(ri);
		printf("%s", temp);
		free(temp);
		printf(" \e[38;5;240m(%s)\e[0m\n", ri->RepositoryUnprocessedOrigin);
	}
	else {
		printf("%s\n", ri->DirectoryName);
	}
	fflush(stdout);
	RepoList* current = ri->SubDirectories;
	int procedSubDirs = 0;
	char* temp;
	asprintf(&temp, "%s%s", parentPrefix, (ri->ParentDirectory != NULL) ? (anotherSameLevelEntryFollows ? "\u2502   " : "    ") : "");
	while (current != NULL)
	{
		procedSubDirs++;
		printTree_internal(current->self, temp, procedSubDirs < ri->SubDirectoryCount);
		current = current->next;
	}
	free(temp);
}

void printTree(RepoInfo* ri) {
	printTree_internal(ri, "", ri->SubDirectoryCount > 1);
}

bool pruneTreeForGit(RepoInfo* ri) {
	bool hasGitInTree = false;
	if (ri->SubDirectories != NULL) {
		RepoList* current = ri->SubDirectories;
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
					ri->SubDirectories = current->next;
				}
				if (current->next != NULL) {
					current->next->prev = current->prev;
				}
				ri->SubDirectoryCount--;
				//printf("removing %s\n", current->self->DirectoryName);
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
			fprintf(fp, "GITHUB	ssh://git@github.com:username\n");
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

int VisibleStrLen(const char* s) {
	int count = 0;
	int idx = 0;
	char c;
	while ((c = s[idx]) != 0x00) {
		//test for zsh prompt stuff
		if (c == '%') {
			if (s[idx + 1] == 'F' && s[idx + 2] == '{') {//%F{...} -> set colour
				while (s[idx] != 0x00 && s[idx] != '}') {
					idx++;
				}
				idx++;
				continue;
			}
			else if (s[idx + 1] == 'f') {//clear colour
				idx += 2;
				continue;
			}
			else if (s[idx + 1] == 'b') {//clear bold
				idx += 2;
				continue;
			}
			else if (s[idx + 1] == 'B') {//set bold
				idx += 2;
				continue;
			}
			else if (s[idx + 1 == '%']) {//escaped %, an actual % sign
				//NOTE: NO continue and ONLY +1 because I WANT to read that
				idx++;
			}
		}
		//ANSI CSI sequences https://en.wikipedia.org/wiki/ANSI_escape_code
		//don't count them at all
		if (c == '\e' && s[idx + 1] == '[') {
			idx += 2;//advance over \e[to test for the rest
			//walk until valid 'final byte' or NULL found
			while (s[idx] != 0x00 && !(s[idx] >= 0x40 && s[idx] <= 0x7E)) {
				idx++;
			}
		}
		//UTF-8 multibyte characters -> only count them once
		if ((c & 0b11000000) == 0b11000000) {
			//begins with 11... -> UTF8
			count++;
#ifdef DEBUG
			printf("counting Unicode character\n");
#endif
			while (s[idx] != 0x00 && (c & 0b11000000) == 0b10000000) {
				//in utf-8 the first byte is 11------ while all following bytes are 10------
				idx++;
			}
		}
		if (c >= 0x20 && c <= 0x7e) {
			count++;
#ifdef DEBUG
			printf("%c (%x) -> %c (%x)\n", c, c, s[idx], s[idx]);
#endif
		}
		idx++;
	}
#ifdef DEBUG
	printf("<" COLOUR_CLEAR "%s" COLOUR_CLEAR "> total -> %i\n", s, count);
#endif
	return count;
}

uint8_t determinePossibleCombinations(int* availableLen, int el0len, int el1len, int el2len, int el3len, int el4len, int el5len, int el6len) {
	uint8_t res = 0;
	if (*availableLen > el0len) {
		res |= 1 << 0;
		*availableLen -= el0len;
	}
	if (*availableLen > el1len) {
		res |= 1 << 1;
		*availableLen -= el1len;
	}
	if (*availableLen > el2len) {
		res |= 1 << 2;
		*availableLen -= el2len;
	}
	if (*availableLen > el3len) {
		res |= 1 << 3;
		*availableLen -= el3len;
	}
	if (*availableLen > el4len) {
		res |= 1 << 4;
		*availableLen -= el4len;
	}
	if (*availableLen > el5len) {
		res |= 1 << 5;
		*availableLen -= el5len;
	}
	if (*availableLen > el6len) {
		res |= 1 << 6;
		*availableLen -= el6len;
	}
	return res;
}

int main(int argc, char** argv)
{
	buf = (char*)malloc(sizeof(char) * 1024);
	bufCurLen = 0;
	if (buf == NULL) {
		printf("couldn't malloc required buffer");
	}

	DoSetup();

	if (Compare(argv[1], "PROMPT")) //show origin info for command prompt
	{
		if (argc != 14) {
			printf("INVAILD PARAMETER COUNT %i\nNEED: PROMPT $pwd $COLUMNS User_at_host TerminalDevice ip_info proxy_info power_info", argc);
			return -1;
		}
		RepoInfo* ri = AllocRepoInfo("", argv[2]);
		TestPathForRepoAndParseIfExists(ri, -1, true, true);
		if (ri == NULL) {
			printf("error at main: TestPathForRepoAndParseIfExists returned null\n");
			return 1;
		}
		AllocUnsetStringsToEmpty(ri);

		int TotalPromptWidth = atoi(argv[3]);
		int ID_UserAtHost = 4;
		int ID_TerminalDevice = 5;
		int ID_Time = 6;
		int ID_TimeZone = 7;
		int ID_LocalIPs = 8;
		int ID_ProxyInfo = 9;
		int ID_PowerState = 10;
		int ID_BackgroundJobs = 11;
		int ID_SHLVL = 12;
		int ID_SSHInfo = 13;

		//if top line too short -> omission order: git submodule location (git_4)(...@/path/to/parent/repo); device (/dev/pts/0); time Zone (UTC+0200 (CEST)); IP info ; git remote info(git_2) (... from displayedorigin)

		//since the output will be sent through an escape sequence processing system to do the colouts and stuff I must replace the literal % for the battery with a %% (the literal % is marked by \a%)
		char* temp = argv[ID_PowerState];
		while (*temp != 0x00) {
			if (*temp == '\a' && *(temp + 1) == '%') {
				*temp = '%';
			}
			temp++;
		}

		TerminateStrOnCR_LF_BEL(argv[4]);
		TerminateStrOnCR_LF_BEL(argv[5]);
		TerminateStrOnCR_LF_BEL(argv[6]);
		TerminateStrOnCR_LF_BEL(argv[7]);
		TerminateStrOnCR_LF_BEL(argv[8]);
		TerminateStrOnCR_LF_BEL(argv[9]);
		TerminateStrOnCR_LF_BEL(argv[10]);
		TerminateStrOnCR_LF_BEL(argv[11]);
		TerminateStrOnCR_LF_BEL(argv[12]);
		TerminateStrOnCR_LF_BEL(argv[13]);

		//if the shell level is 1, don't print it, only if slehh level >1 show it
		if (Compare(argv[ID_SHLVL], " [1]")) {
			argv[ID_SHLVL][0] = 0x00;
		}

		int lens[argc];
		for (int i = 0;i < 4;i++) {//just to zero fill the unused segment
			lens[i] = 0;
		}
		for (int i = 4;i < argc;i++) {
			lens[i] = VisibleStrLen(argv[i]);
		}


		char* gitSegment1_BaseMarkerStart = "";
		char* gitSegment2_parentRepoLoc = gitSegment1_BaseMarkerStart;//just an empty default
		char* gitSegment3_BaseMarkerEnd = gitSegment1_BaseMarkerStart;
		char* gitSegment4_remoteinfo = gitSegment1_BaseMarkerStart;
		char* gitSegment5_gitStatus = gitSegment1_BaseMarkerStart;
		int gitSegment1_BaseMarkerStart_len = 0;
		int gitSegment2_parentRepoLoc_len = 0;
		int gitSegment3_BaseMarkerEnd_len = 0;
		int gitSegment4_remoteinfo_len = 0;
		int gitSegment5_gitStatus_len = 0;

		if (ri->isGit) {
			asprintf(&gitSegment1_BaseMarkerStart, " [" COLOUR_GIT_BARE "%s" COLOUR_GIT_INDICATOR "GIT%s", ri->isBare ? "BARE " : "", ri->isSubModule ? "-SM" : "");//[%F{006}BARE %F{002}GIT-SM
			if (ri->isSubModule) {
				asprintf(&gitSegment2_parentRepoLoc, COLOUR_CLEAR "@" COLOUR_GIT_PARENT "%s" COLOUR_CLEAR, ri->parentRepo);
			}
			asprintf(&gitSegment3_BaseMarkerEnd, COLOUR_CLEAR "] " COLOUR_GIT_NAME "%s" COLOUR_CLEAR " on "COLOUR_GIT_BRANCH "%s" COLOUR_CLEAR, ri->RepositoryName, ri->branch);
			asprintf(&gitSegment4_remoteinfo, " from " COLOUR_GIT_ORIGIN "%s" COLOUR_CLEAR, ri->RepositoryDisplayedOrigin);

			//this version of gitSegement5 is a vastly cut-down versio, only indicating 'dirty' or 'not dirty' while the whole segment below makes a much more detailed statement, but the unicode chars may be useful
			//asprintf(&gitSegment5_gitStatus, COLOUR_GIT_MODIFIED " %s" COLOUR_CLEAR "", ri->DirtyWorktree ? "\u2573\u2718" : "");

			gitSegment5_gitStatus = ConstructGitStatusString(ri);

			gitSegment1_BaseMarkerStart_len = VisibleStrLen(gitSegment1_BaseMarkerStart);
			gitSegment2_parentRepoLoc_len = VisibleStrLen(gitSegment2_parentRepoLoc);
			gitSegment3_BaseMarkerEnd_len = VisibleStrLen(gitSegment3_BaseMarkerEnd);
			gitSegment4_remoteinfo_len = VisibleStrLen(gitSegment4_remoteinfo);
			gitSegment5_gitStatus_len = VisibleStrLen(gitSegment5_gitStatus);
		}

#ifdef DEBUG
		printf("%i\n", TotalPromptWidth);
		printf("%i <%s>\n", gitSegment1_BaseMarkerStart_len, gitSegment1_BaseMarkerStart);
		printf("%i <%s>\n", gitSegment2_parentRepoLoc_len, gitSegment2_parentRepoLoc);
		printf("%i <%s>\n", gitSegment3_BaseMarkerEnd_len, gitSegment3_BaseMarkerEnd);
		printf("%i <%s>\n", gitSegment4_remoteinfo_len, gitSegment4_remoteinfo);
		printf("%i <%s>\n", gitSegment5_gitStatus_len, gitSegment5_gitStatus);
		for (int i = 4;i < argc;i++) {
			printf("%i <%s>\n", lens[i], argv[i]);
	}
#endif


		int RemainingPromptWidth = TotalPromptWidth - (lens[ID_UserAtHost] + lens[ID_SHLVL] + gitSegment1_BaseMarkerStart_len + gitSegment3_BaseMarkerEnd_len + gitSegment5_gitStatus_len + lens[ID_Time] + lens[ID_ProxyInfo] + lens[ID_PowerState] + 1);

		uint8_t AdditionalElementAvailabilityPackedBool = determinePossibleCombinations(&RemainingPromptWidth, gitSegment4_remoteinfo_len, lens[ID_LocalIPs], lens[ID_TimeZone], lens[ID_TerminalDevice], gitSegment2_parentRepoLoc_len, lens[ID_BackgroundJobs], lens[ID_SSHInfo]);


		//if the seventh-prioritized element (ssh connection info) has space, print it "<SSH: 100.85.145.164:49567 -> 192.168.0.220:22> "
		if (AdditionalElementAvailabilityPackedBool & (1 << 6)) {
			printf("%s", argv[ID_SSHInfo]);
		}

		//print username and machine "user@hostname"
		printf("%s", argv[ID_UserAtHost]);

		//if the fourth-prioritized element (the line/terminal device has space, append it to the user@machine) ":/dev/pts/0"
		if (AdditionalElementAvailabilityPackedBool & (1 << 3)) {
			printf("%s", argv[ID_TerminalDevice]);
		}

		//append the SHLVL (how many shells are nested, ie how many Ctrl+D are needed to properly exit), only shown if >=2 " [2]"
		//also print the first bit of the git indication (is submodule or not) " [GIT-SM"
		printf("%s" COLOUR_CLEAR "%s", argv[ID_SHLVL], gitSegment1_BaseMarkerStart);

		//if the fifth-prioritized element (the the location of the parent repo, if it exists, ie if this is submodule) "@/mnt/e/CODE"
		if ((AdditionalElementAvailabilityPackedBool & (1 << 4))) {
			printf("%s", gitSegment2_parentRepoLoc);
		}

		//print the repo name and branch including the closing bracket arount the git indication "] ShellTools on master"
		printf("%s", gitSegment3_BaseMarkerEnd);

		//if the first-prioritized element (the git remote origin information) has space, print it " from ssh:git@someprivateurl.de:1234"
		if ((AdditionalElementAvailabilityPackedBool & (1 << 0))) {
			printf("%s", gitSegment4_remoteinfo);
		}

		//print the last bit of the git indicator (commits to pull, push, stashes, merge-conflict-files, staged changes, non-staged changes, untracked files) " {77⇣ 0⇡} <5M 10+ 3* 4?>"
		printf("%s", gitSegment5_gitStatus);

		//fill the empty space between left and right side
		for (int i = 0;i < RemainingPromptWidth;i++) {
			printf("-");
		}

		//print the time in HH:mm:ss "21:24:31"
		printf("%s", argv[ID_Time]);

		//if the third-prioritized element (timezone info) has space, print it " UTC+0200 (CEST)"
		if ((AdditionalElementAvailabilityPackedBool & (1 << 2))) {
			printf("%s", argv[ID_TimeZone]);
		}

		//if a proxy is configured, show it (A=Apt, H=http(s), F=FTP, N=None) " [AHF]"
		printf("%s", argv[ID_ProxyInfo]);

		//if the second prioritized element (local IP addresses) has space, print it " eth0:127.0.0.1 wifi0:127.0.0.2"
		if ((AdditionalElementAvailabilityPackedBool & (1 << 1))) {
			printf("%s", argv[ID_LocalIPs]);
		}

		//if the sixth-prioritized element (background tasks) has space, print it " {1S-  2S+}"
		if ((AdditionalElementAvailabilityPackedBool & (1 << 5))) {
			printf("%s", argv[ID_BackgroundJobs]);
		}

		//print the battery state (the first unicode char can be any of ▲,≻,▽ or ◊[for not charging,but not dischargind]), while the second unicode char indicates the presence of AC power "≻ 100% ↯"
		printf("%s", argv[ID_PowerState]);


		if (ri->isGit) {
			free(gitSegment1_BaseMarkerStart);
			if (ri->isSubModule) {
				free(gitSegment2_parentRepoLoc);
			}
			free(gitSegment3_BaseMarkerEnd);
			free(gitSegment4_remoteinfo);
			free(gitSegment5_gitStatus);
		}
		DeallocRepoInfoStrings(ri);
		free(ri);

		return 0;
}
	else if (Compare(argv[1], "SET") || Compare(argv[1], "SHOW"))
	{
		int requestedNewOrigin = -1;
		if (Compare(argv[1], "SET")) {//a change was requested
			for (int i = 0;i < numLOCS;i++) {
				if (Compare(argv[3], NAMES[i])) {//found requested new origin
					requestedNewOrigin = i;
					break;
				}
			}
			if (requestedNewOrigin == -1) {
				printf("new origin specification " COLOUR_GIT_ORIGIN "'%s'" COLOUR_CLEAR " is unknown. available origin specifications are:\n", argv[3]);
				requestedNewOrigin = -2;//this is a special flag to drop into LIST ORIGINS and exit (as error handling mechanism)
			}
		}

		if (requestedNewOrigin == -2) {//figue out a nicer thing
			for (int i = 0;i < numLOCS;i++) {
				printf(COLOUR_GIT_ORIGIN "%s" COLOUR_CLEAR " (-> %s)\n", NAMES[i], LOCS[i]);
			}
			return 0;
		}

		bool ThoroughSearch = Compare(argv[argc - 1], "THOROUGH");
		printf("Performing %s search for repos on %s", ThoroughSearch ? "thorough" : "quick", argv[2]);
		if (requestedNewOrigin != -1) {
			printf(" and setting my repos to %s", NAMES[requestedNewOrigin]);
		}
		printf("\n\n");

		RepoInfo* treeroot = CreateDirStruct("", argv[2], requestedNewOrigin, ThoroughSearch);
		pruneTreeForGit(treeroot);
		if (requestedNewOrigin != -1) {
			printf("\n");
		}
		printTree(treeroot);
		printf("\n");
	}
	free(buf);
}
