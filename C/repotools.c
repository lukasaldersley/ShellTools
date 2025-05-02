#/*
TargetDir="$ST_CFG"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename "$0" .c).elf"
printf "compiling %s into %s/%s" "$0" "$TargetDir" "$TargetName"
#shellcheck disable=SC2086 #in this case I DO want word splitting to happen at $1
gcc -O3 -std=c2x -Wall "$(realpath "$(dirname "$0")")"/commons.c "$(realpath "$(dirname "$0")")"/gitfunc.c "$0" -o "$TargetDir/$TargetName" $1
#I WANT to be able to do things like ./repotools.c -DPROFILING to add the compiler flag profiling but ALSO stuff like ./repotools "-DDEBUG -DPROFILING" to add both profiling and debug
printf " -> \e[32mDONE\e[0m(%s)\n" $?
exit
*/

#include "commons.h"
#include "gitfunc.h"
#include <regex.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <getopt.h>
#include <signal.h>

#define COLOUR_TERMINAL_DEVICE "\e[38;5;242m"
#define COLOUR_SHLVL "\e[0m"
#define COLOUR_USER "\e[38;5;010m"
#define COLOUR_USER_AT_HOST "\e[38;5;007m"
#define COLOUR_HOST "\e[38;5;033m"

#define maxGroups 10
regmatch_t CapturedResults[maxGroups];
regex_t branchParsingRegex;

typedef enum { IP_MODE_NONE, IP_MODE_LEGACY, IP_MODE_STANDALONE } IP_MODE;

#define MaxLocations 32
char* NAMES[MaxLocations];
char* LOCS[MaxLocations];
char* GitHubs[MaxLocations];
uint8_t numGitHubs = 0;
uint8_t numLOCS = 0;

bool CONFIG_LOWPROMPT_PATH_LIMIT = true;
int CONFIG_LOWPROMPT_PATH_MAXLEN = -3;
bool CONFIG_LOWPROMPT_RETCODE = true;
bool CONFIG_LOWPROMPT_RETCODE_DECODE = true;
bool CONFIG_LOWPROMPT_TIMER = true;

bool CONFIG_PROMPT_OVERALL_ENABLE = true;

bool CONFIG_PROMPT_SSH = true;
bool CONFIG_PROMPT_TERMINAL_DEVICE = true;
bool CONFIG_PROMPT_TIME = true;
bool CONFIG_PROMPT_TIMEZONE = true;
bool CONFIG_PROMPT_DATE = true;
bool CONFIG_PROMPT_CALENDARWEEK = true;
bool CONFIG_PROMPT_PROXY = true;
bool CONFIG_PROMPT_NETWORK = true;
bool CONFIG_PROMPT_JOBS = true;
bool CONFIG_PROMPT_POWER = true;
bool CONFIG_PROMPT_GIT = true;
bool CONFIG_PROMPT_USER = true;
bool CONFIG_PROMPT_HOST = true;
char CONFIG_PROMPT_FILLER_CHAR = '-';

bool CONFIG_PROMPT_NET_IFACE = true;
bool CONFIG_PROMPT_NET_ADDITIONAL = true;
bool CONFIG_PROMPT_NET_ROUTE = true;
bool CONFIG_PROMPT_NET_LINKSPEED = true;


int CONFIG_GIT_MAXBRANCHES = -2;
bool CONFIG_GIT_WARN_BRANCH_LIMIT = true;
bool CONFIG_GIT_REPOTYPE = true;
bool CONFIG_GIT_REPOTYPE_PARENT = true;
bool CONFIG_GIT_REPONAME = true;
bool CONFIG_GIT_BRANCHNAME;
bool CONFIG_GIT_BRANCH_OVERVIEW;
bool CONFIG_GIT_BRANCHSTATUS;
bool CONFIG_GIT_REMOTE;
bool CONFIG_GIT_COMMIT_OVERVIEW;
bool CONFIG_GIT_LOCALCHANGES;

int CONFIG_PROMPT_GIT_BRANCHLIMIT = 25;
bool CONFIG_PROMPT_GIT_WARN_BRANCHLIMIT = true;
bool CONFIG_PROMPT_GIT_REPOTYPE = true;
bool CONFIG_PROMPT_GIT_REPOTYPE_PARENT = true;
bool CONFIG_PROMPT_GIT_REPONAME = true;
bool CONFIG_PROMPT_GIT_BRANCHNAME = true;
bool CONFIG_PROMPT_GIT_BRANCHINFO = true;
bool CONFIG_PROMPT_GIT_BRANCHSTATUS = true;
bool CONFIG_PROMPT_GIT_REMOTE = true;
bool CONFIG_PROMPT_GIT_COMMITS = true;
bool CONFIG_PROMPT_GIT_GITSTATUS = true;

bool CONFIG_LSGIT_WARN_BRANCHLIMIT = true;
int  CONFIG_LSGIT_QUICK_BRANCHLIMIT = 10;
bool CONFIG_LSGIT_QUICK_WARN_BRANCHLIMIT = true;
bool CONFIG_LSGIT_QUICK_REPOTYPE = true;
bool CONFIG_LSGIT_QUICK_REPOTYPE_PARENT = true;
bool CONFIG_LSGIT_QUICK_REPONAME = true;
bool CONFIG_LSGIT_QUICK_BRANCHNAME = true;
bool CONFIG_LSGIT_QUICK_BRANCHINFO = true;
bool CONFIG_LSGIT_QUICK_BRANCHSTATUS = true;
bool CONFIG_LSGIT_QUICK_REMOTE = true;
bool CONFIG_LSGIT_QUICK_COMMITS = true;
bool CONFIG_LSGIT_QUICK_GITSTATUS = true;
int  CONFIG_LSGIT_THOROUGH_BRANCHLIMIT = -1;
bool CONFIG_LSGIT_THOROUGH_WARN_BRANCHLIMIT = true;
bool CONFIG_LSGIT_THOROUGH_REPOTYPE = true;
bool CONFIG_LSGIT_THOROUGH_REPOTYPE_PARENT = true;
bool CONFIG_LSGIT_THOROUGH_REPONAME = true;
bool CONFIG_LSGIT_THOROUGH_BRANCHNAME = true;
bool CONFIG_LSGIT_THOROUGH_BRANCHINFO = true;
bool CONFIG_LSGIT_THOROUGH_BRANCHSTATUS = true;
bool CONFIG_LSGIT_THOROUGH_REMOTE = true;
bool CONFIG_LSGIT_THOROUGH_COMMITS = true;
bool CONFIG_LSGIT_THOROUGH_GITSTATUS = true;

bool DIPFALSCHEISSER_WARNINGS = false;

bool CheckBranching(RepoInfo* ri) {
	//this method checks the status of all branches on a given repo
	//and then computes how many differ, howm many up to date, how many branches local-only, how many branches remote-only
	BranchListSorted* ListBase = NULL;

	int size = 1024;
	char* result = (char*)malloc(sizeof(char) * size);
	if (result == NULL) ABORT_NO_MEMORY;
	char* command;
	if (asprintf(&command, "git -C \"%s\" branch -vva", ri->DirectoryPath) == -1) ABORT_NO_MEMORY;
	FILE* fp = popen(command, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "failed running process %s\n", command);
	}
	else {
		int branchcount = 0;
		while (fgets(result, size - 1, fp) != NULL)
		{
			//iterating over list of branches (remote and local seperatly)
			TerminateStrOn(result, DEFAULT_TERMINATORS);
			branchcount++;
			if (CONFIG_GIT_MAXBRANCHES != -1 && branchcount > CONFIG_GIT_MAXBRANCHES) {
				free(result);
				pclose(fp);
				free(command);
				if (CONFIG_GIT_WARN_BRANCH_LIMIT) {
					fprintf(stderr, "more than %i branches for repo %s -> aborting branchhandling\n", CONFIG_GIT_MAXBRANCHES, ri->DirectoryPath);
				}
				while (ListBase != NULL) {
					BranchListSorted* temp = ListBase;
					ListBase = ListBase->next;
					free(temp);
				}
				//printf("BRANCHING: ABORT\r");
				//fflush(stdout);
				return false;
			}
			//printf("\n\n%s\n", result);
			fflush(stdout);
			int RegexReturnCode = regexec(&branchParsingRegex, result, maxGroups, CapturedResults, 0);
			//man regex (3): regexec() returns zero for a successful match or REG_NOMATCH for failure.
			if (RegexReturnCode == 0)
			{
				char* bname = NULL;
				char* hash = NULL;
				bool rm = false;
				for (unsigned int GroupID = 0; GroupID < maxGroups; GroupID++)
				{
					if (CapturedResults[GroupID].rm_so == (size_t)-1) {
						break;  // No more groups
					}
					regoff_t start, end;
					start = CapturedResults[GroupID].rm_so;
					end = CapturedResults[GroupID].rm_eo;
					int len = end - start;

					char* x = malloc(sizeof(char) * (len + 1));
					if (x == NULL) ABORT_NO_MEMORY;
					strncpy(x, result + start, len);
					x[len] = 0x00;
					if (GroupID == 1) {
						int offs = 0;
						if (StartsWith(x, "remotes/")) {
							//offs = LastIndexOf(x, '/') + 1; //if the branch name contains / such as feature/SOMETHING that would fail.
							//now I check not the text after the last / but the text after remotes/***/ to allow remotes other than origin
							offs = NextIndexOf(x, '/', 8) + 1;
							rm = true;
						}
						bname = malloc(sizeof(char) * ((len - offs) + 1));
						if (bname == NULL) ABORT_NO_MEMORY;
						strncpy(bname, result + start + offs, len - offs);
						bname[len - offs] = 0x00;
					}
					else if (GroupID == 2) {
						hash = malloc(sizeof(char) * (len + 1));
						if (hash == NULL) ABORT_NO_MEMORY;
						strncpy(hash, result + start, len);
						hash[len] = 0x00;
					}
					free(x);
				}
				if (bname != NULL && hash != NULL) {
					ListBase = InsertIntoBranchListSorted(ListBase, bname, hash, rm);
					free(bname);
					free(hash);
				}
				else {
					printf("found something non matching\n");
					fflush(stdout);
				}
			}
		}
	}

	BranchListSorted* ptr = ListBase;
	BranchListSorted* LastKnown = NULL;
	while (ptr != NULL) {
		LastKnown = ListBase;
		if (ptr->branchinfo.CommitHashRemote != NULL) {
			ptr->branchinfo.IsMergedRemote = IsMerged(ri->DirectoryPath, ptr->branchinfo.CommitHashRemote);
		}
		else {
			//If the branch only exists in one place, the other shall be counted as fully merged
			//the reason is: if BOTH are fully merged I consider the branch legacy,
			//but only if BOTH are, so to make a remote - only fully merged branch count as such, the non - existing local branch must count as merged
			ptr->branchinfo.IsMergedRemote = true;
		}
		if (ptr->branchinfo.CommitHashLocal != NULL) {
			ptr->branchinfo.IsMergedLocal = IsMerged(ri->DirectoryPath, ptr->branchinfo.CommitHashLocal);
		}
		else {
			ptr->branchinfo.IsMergedLocal = true;
		}

		//printf("{%s: %s%c|%s%c}\n",
		//	(ptr->branchinfo.BranchName != NULL ? ptr->branchinfo.BranchName : "????"),
		//	(ptr->branchinfo.CommitHashLocal != NULL ? ptr->branchinfo.CommitHashLocal : "---"),
		//	ptr->branchinfo.IsMergedLocal ? 'M' : '!',
		//	(ptr->branchinfo.CommitHashRemote != NULL ? ptr->branchinfo.CommitHashRemote : "---"),
		//	ptr->branchinfo.IsMergedRemote ? 'M' : '!');


		if (ptr->branchinfo.CommitHashLocal == NULL && ptr->branchinfo.CommitHashRemote != NULL && !ptr->branchinfo.IsMergedRemote) {//remote-only, non-merged
			ri->CountRemoteOnlyBranches++;
		}

		if (ptr->branchinfo.CommitHashLocal != NULL && ptr->branchinfo.CommitHashRemote == NULL) {
			ri->CountLocalOnlyBranches++;
			//If I CURRENTLY am ON THIS branch, indicate it as {NEW}
			if (Compare(ptr->branchinfo.BranchName, ri->branch)) {
				ri->CheckedOutBranchIsNotInRemote = true;
			}
		}

		if (ptr->branchinfo.IsMergedLocal && ptr->branchinfo.IsMergedRemote) {
			ri->CountFullyMergedBranches++;
		}
		else {
			ri->CountActiveBranches++;
		}

		if (ptr->branchinfo.CommitHashLocal != NULL && ptr->branchinfo.CommitHashRemote != NULL) {
			if (!Compare(ptr->branchinfo.CommitHashLocal, ptr->branchinfo.CommitHashRemote)) {
				ri->CountUnequalBranches++;
			}
		}
		ptr = ptr->next;
	}
	//free from the back forward
	while (LastKnown != NULL) {
		BranchListSorted* temporary = LastKnown->prev;
		free(LastKnown);
		LastKnown = temporary;
	}
	ListBase = NULL;

	//printf("/%i+%i: (%i⇣ %i⇡ %i⇵)\n", ri->CountActiveBranches, ri->CountFullyMergedBranches, ri->CountRemoteOnlyBranches, ri->CountLocalOnlyBranches, ri->CountUnequalBranches);
	free(result);
	pclose(fp);
	free(command);
	return true;
}

/*
	I can get the list of all submodules by using
	git -C *TARGET_FOLDER* submodule status --recursive | sed -n 's|.[0-9a-fA-F]\+ \([^ ]\+\)\( .*\)\?|\1|p'
	This would give me a list of submodules of the current repo. I could then compare the output to when I come across them.
	This method provides me with a list of relative paths
	*/

bool TestPathForRepoAndParseIfExists(RepoInfo* ri, int desiredorigin, bool DoProcessWorktree, bool BeThorough) {
	//the return value is just: has this successfully executed

	char* cmd;
	if (BeThorough || DoProcessWorktree) {
		if (asprintf(&cmd, "git -C \"%s\" rev-parse --is-bare-repository 2>/dev/null", ri->DirectoryPath) == -1) ABORT_NO_MEMORY;
		char* bareRes = ExecuteProcess_alloc(cmd);
		free(cmd);
		if (bareRes == NULL) {
			//Encountered error -> quit
			DeallocRepoInfoStrings(ri);
			free(ri);
			return false;
		}
		TerminateStrOn(bareRes, DEFAULT_TERMINATORS);
		ri->isBare = Compare(bareRes, "true");
		free(bareRes);
		bareRes = NULL;
		ri->isGit = ri->isBare;//initial value

		if (!ri->isBare) {
			if (asprintf(&cmd, "git -C \"%s\" rev-parse --show-toplevel 2>/dev/null", ri->DirectoryPath) == -1) ABORT_NO_MEMORY;
			char* tlres = ExecuteProcess_alloc(cmd);
			TerminateStrOn(tlres, DEFAULT_TERMINATORS);
			free(cmd);
			ri->isGit = Compare(ri->DirectoryPath, tlres);
			//printf("dir %s tl: %s (%i) dpw: %i\n", ri->DirectoryPath, tlres, ri->isGit, DoProcessWorktree);
			free(tlres);
			if (DoProcessWorktree && !(ri->isGit)) {
				//usually we are done at this stage (only treat as git if in toplevel, but this is for the prompt substitution where it needs to work anywhere in git)
				if (asprintf(&cmd, "git -C \"%s\" rev-parse --is-inside-work-tree 2>/dev/null", ri->DirectoryPath) == -1) ABORT_NO_MEMORY;
				char* wtres = ExecuteProcess_alloc(cmd);
				TerminateStrOn(wtres, DEFAULT_TERMINATORS);
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
	if (asprintf(&cmd, "git -C  \"%1$s\" symbolic-ref --short HEAD 2>/dev/null || git -C \"%1$s\" describe --tags --exact-match HEAD 2>/dev/null || git -C \"%1$s\" rev-parse --short HEAD", ri->DirectoryPath) == -1) ABORT_NO_MEMORY;
	ri->branch = ExecuteProcess_alloc(cmd);
	TerminateStrOn(ri->branch, DEFAULT_TERMINATORS);
	free(cmd);
	cmd = NULL;

	//if 'git rev-parse --show-superproject-working-tree' outputs NOTHING, a repo is standalone, if there is output it will point to the parent repo
	if (asprintf(&cmd, "git -C \"%s\" rev-parse --show-superproject-working-tree", ri->DirectoryPath) == -1) ABORT_NO_MEMORY;
	char* temp = ExecuteProcess_alloc(cmd);
	AbbreviatePath(&(ri->parentRepo), temp, 15, 1, 2);
	free(temp);
	temp = NULL;
	TerminateStrOn(ri->parentRepo, DEFAULT_TERMINATORS);
	ri->isSubModule = !((ri->parentRepo)[0] == 0x00);
	free(cmd);

	if ((CONFIG_GIT_BRANCHSTATUS || CONFIG_GIT_BRANCH_OVERVIEW) && CONFIG_GIT_MAXBRANCHES > 0) {
		CheckBranching(ri);
	}

	if (!ri->isBare && (CONFIG_GIT_LOCALCHANGES || CONFIG_GIT_COMMIT_OVERVIEW)) {
		//if I need neither the overview over commits nor local changes, I can skip this thereby significantly speeding up the whole process
		CheckExtendedGitStatus(ri);
		ri->DirtyWorktree = !(ri->ActiveMergeFiles == 0 && ri->ModifiedFiles == 0 && ri->StagedChanges == 0);
	}

	if (desiredorigin != -1 || CONFIG_GIT_REMOTE || CONFIG_GIT_REPONAME) {
		//I only need to concern myself with the remote and reponame If they are either directly requested or implicity needed for setGitBase
		if (asprintf(&cmd, "git -C \"%s\" ls-remote --get-url origin", ri->DirectoryPath) == -1) ABORT_NO_MEMORY;
		ri->RepositoryUnprocessedOrigin = ExecuteProcess_alloc(cmd);
		free(cmd);
		if (ri->RepositoryUnprocessedOrigin == NULL) {
			DeallocRepoInfoStrings(ri);
			free(ri);
			return false;
		}
		cmd = NULL;
		TerminateStrOn(ri->RepositoryUnprocessedOrigin, DEFAULT_TERMINATORS);
		if (Compare(ri->RepositoryUnprocessedOrigin, "origin")) {
			//if git ls-remote --get-url origin returns 'origin' it means either the folder is not a git repository OR it's a repository without remote (local only)
			//in this case since I already checked this IS a repo, it MUST be a repo without remote
			ri->HasRemote = false;
			if (asprintf(&ri->RepositoryDisplayedOrigin, "NO_REMOTE") == -1) ABORT_NO_MEMORY;
			int i = 0;
			const char* tempPtr = ri->DirectoryPath;
			while (ri->DirectoryPath[i] != 0x00) {
				if (ri->DirectoryPath[i] == '/') {
					tempPtr = ri->DirectoryPath + i + 1;
				}
				i++;
			}
			if (asprintf(&ri->RepositoryName, "%s", tempPtr) == -1) ABORT_NO_MEMORY;
			return true;
		}
		else {
			//has some form of remote
			ri->HasRemote = true;
			char* FixedProtoOrigin = FixImplicitProtocol(ri->RepositoryUnprocessedOrigin);

			// input: repoToTest, the path of a repo. if it is one of the defined repos, return that, if it's not, produce the short notation
			// basically this should produce the displayed name for the repo in the output buffer and additionally indicate if it's a known one
			ri->RepositoryOriginID = -1;
			for (int i = 0; i < numLOCS; i++)
			{
				//fprintf(stderr, "%s > %s testing against %s(%s)\n", ri->RepositoryUnprocessedOrigin, FixedProtoOrigin, LOCS[i], NAMES[i]);
				if (StartsWith(FixedProtoOrigin, LOCS[i]))
				{
					//fprintf(stderr, "\tSUCCESS\n");
					ri->RepositoryOriginID = i;
					if (asprintf(&ri->RepositoryDisplayedOrigin, "%s", NAMES[i]) == -1) ABORT_NO_MEMORY;
					break;
				}
			}

			char* sedCmd;
			//this regex is basically:
			//^(?<proto>[-\w+])://(?:(?<user>[-\w]+)@)?(?<host>[-\.\w]+)(?::(?<port>\d+))?(?:[:/](?<remotePath_GitHubUser>[-\w]+))?.*/(?<reponame>[-\w]+)(?:\.git/?)?$ //this is the debuggin version for regex101.com (PCRE<7.3, delimiter ~)
			//sed does not have non-capturing groups, so all non-capturing groups are included in the group count
			//the mapping of sed-groups to the regex101 groups is as follows:
			//1->protoc
			//2->Non-capturing
			//3->user
			//4->host
			//5->Non-capturing(:port)
			//6->port
			//7->Non-capturing (:remotepath or /gitHubUser)
			//8->remotePath_GitHubUser (if not GitHub, it's the path on remote)
			//9->reponame
			//10->Non-Capturing(.git)
			//DO NOT CHANGE THIS REGEX WITHOUT UPDATING THE Regex101 VARIANT, THE GROUP DEFINITIONS AND THE DESCRIPTION
			if (asprintf(&sedCmd, "echo \"%s\" | sed -nE 's~^([-a-zA-Z0-9_]+)://(([-a-zA-Z0-9_]+)@){0,1}([-0-9a-zA-Z_\\.]+)(:([0-9]+)){0,1}([:/]([-0-9a-zA-Z_]+)){0,1}.*/([-0-9a-zA-Z_]+)(\\.git/{0,1}){0,1}$~\\1|\\3|\\4|\\6|\\8|\\9~p'", FixedProtoOrigin) == -1) ABORT_NO_MEMORY;
			//I take the capturing groups and paste them into a | seperated sting. There's 6 words (5 |), so I'll need 6 pointers into this memory area to resolve the six words
			const int REPO_ORIGIN_WORDS_IN_STRING = 6;
			const int REPO_ORIGIN_GROUP_PROTOCOL = 0;
			const int REPO_ORIGIN_GROUP_USER = 1;
			const int REPO_ORIGIN_GROUP_Host = 2;
			const int REPO_ORIGIN_GROUP_PORT = 3;
			const int REPO_ORIGIN_GROUP_GitHubUser = 4; /*(if not GitHub, it's the path on remote)*/
			const int REPO_ORIGIN_GROUP_RepoName = 5;
			char* sedRes = ExecuteProcess_alloc(sedCmd);
			TerminateStrOn(sedRes, DEFAULT_TERMINATORS);
			if (sedRes[0] == 0x00) {//sed output was empty -> it must be a local repo, just parse the last folder as repo name and the rest as parentrepopath
				// local repo
				if (ri->RepositoryOriginID == -1)
				{
					AbbreviatePath(&(ri->RepositoryDisplayedOrigin), FixedProtoOrigin, 15, 2, 2);
					//if (asprintf(&ri->RepositoryDisplayedOrigin, "%s", FixedProtoOrigin) == -1)ABORT_NO_MEMORY;
				}
				const char* tempptr = FixedProtoOrigin;
				char* walker = FixedProtoOrigin;
				while (*walker != 0x00) {
					if (*walker == '/') {
						tempptr = walker + 1;
					}
					walker++;
				}
				if (asprintf(&ri->RepositoryName, "%s", tempptr) == -1) ABORT_NO_MEMORY;

				// in the .sh implementation I had used realpath relative to pwd to "shorten" the path, but I think it'd be better if I properly regex this up or something.
				// like /folder/folder/[...]/folder/NAME or /folder/[...]/NAME, though if the path is short enough, I'd like the full path
				// local repos $(realpath -q -s --relative-to="argv[2]" "$( echo "$fulltextremote" | grep -v ".\+://")" "")
				//this has the issue of always producing a relative path, though if the path is defined as absolute that should be reflected. Therefore see the implementation of AbbreviatePath in commons.c

				// for locals: realpath -q -s --relative-to="argv[2]" "Input"
				//local repo
			}
			else {
				//the sed command was not empty therefore it matched as a remote thing and needs to be parsed
				char* ptrs[REPO_ORIGIN_WORDS_IN_STRING];
				ptrs[0] = sedRes;
				char* workingPointer = sedRes;
				int NextWordPointer = 0;
				while (*workingPointer != 0x00 && NextWordPointer < (REPO_ORIGIN_WORDS_IN_STRING - 1)) {//since I set NextWordPointer+1^I need to stop at WORDS-2=== 'x < (Words-1)'
					if (*workingPointer == '|') {
						//I found a seperator -> set string terminator for current string
						*workingPointer = 0x00;
						//if there's anything after the seperator, set the start point for the next string
						if (*(workingPointer + 1) != 0x00) {
							NextWordPointer++;
							ptrs[NextWordPointer] = (workingPointer + 1);
						}
					}
					workingPointer++;
				}

				//I take the base name of the remote rep from this parsed string regardless of the fact if it is a LOCLANET/GLOBAL or a known GitHub derivative something unknown
				if (*ptrs[REPO_ORIGIN_GROUP_RepoName] != 0x00) {//repo name
					if (asprintf(&ri->RepositoryName, "%s", ptrs[REPO_ORIGIN_GROUP_RepoName]) == -1) ABORT_NO_MEMORY;
				}

				//the rest of this only makes sense for stuff that's NOT LOCALNET/GLOBAL etc.
				if (ri->RepositoryOriginID == -1)//not a known repo origin (ie not from LOCALNET, NONE etc)
				{
					const int OriginLen = 255;
					ri->RepositoryDisplayedOrigin = (char*)malloc(sizeof(char) * OriginLen + 1);
					if (ri->RepositoryDisplayedOrigin == NULL) ABORT_NO_MEMORY;
					int currlen = 0;
					currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_PROTOCOL], OriginLen - currlen);//proto
					currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ":", OriginLen - currlen);//:
					if (!Compare(ptrs[REPO_ORIGIN_GROUP_USER], "git") && Compare(ptrs[REPO_ORIGIN_GROUP_PROTOCOL], "ssh")) {//if name is NOT git then print it but only print if it was ssh
						currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_USER], OriginLen - currlen);//username
						currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, "@", OriginLen - currlen);//@
					}
					currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_Host], OriginLen - currlen);//host
					if (*ptrs[3] != 0x00) {//if port is given print it
						currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ":", OriginLen - currlen);//:
						currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_PORT], OriginLen - currlen);//username
					}
					if (*ptrs[4] != 0x00) {//host is github or gitlab and I can parse a github username also add it
						bool knownServer = false;
						int i = 0;
						while (i < numGitHubs && knownServer == false) {
							knownServer = Compare(ptrs[REPO_ORIGIN_GROUP_Host], GitHubs[i]);
							i++;
						}
						if (knownServer) {
							currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ":", OriginLen - currlen);//:
							currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_GitHubUser], OriginLen - currlen);//service username
						}
					}
					*(ri->RepositoryDisplayedOrigin + (currlen < OriginLen ? currlen : OriginLen)) = 0x00; //ensure nullbyte
				}
				for (int i = 0;i < REPO_ORIGIN_WORDS_IN_STRING;i++) {
					ptrs[i] = NULL;//to prevent UseAfterFree vulns
				}
			}
			free(sedRes);
			sedRes = NULL;

			//once I have the current and new repo origin IDs perform the change
			if (desiredorigin != -1 && ri->RepositoryOriginID != -1 && ri->RepositoryOriginID != desiredorigin) {

				//change
				ri->RepositoryOriginID_PREVIOUS = ri->RepositoryOriginID;
				if (ri->RepositoryUnprocessedOrigin_PREVIOUS == NULL) { free(ri->RepositoryUnprocessedOrigin_PREVIOUS); }
				ri->RepositoryUnprocessedOrigin_PREVIOUS = ri->RepositoryUnprocessedOrigin;
				ri->RepositoryOriginID = desiredorigin;
				if (asprintf(&ri->RepositoryUnprocessedOrigin, "%s/%s", LOCS[ri->RepositoryOriginID], ri->RepositoryName) == -1) ABORT_NO_MEMORY;
				char* changeCmd;
				if (asprintf(&changeCmd, "git -C \"%s\" remote set-url origin %s", ri->DirectoryPath, ri->RepositoryUnprocessedOrigin) == -1) ABORT_NO_MEMORY;
				printf("%s\n", changeCmd);
				char* deleteme = ExecuteProcess_alloc(changeCmd);
				if (deleteme != NULL)free(deleteme);//just to prevent mem leak
				deleteme = NULL;//prevent UseAfterFree
				free(changeCmd);
			}
		}
	}

	return true;
}

RepoInfo* CreateDirStruct(const char* directoryPath, const char* directoryName, int newRepoSpec, bool BeThorough) {
	RepoInfo* ri = AllocRepoInfo(directoryPath, directoryName);
	if (BeThorough) {
		//If I am searching thorough I need to know if I encountered a bare repo, so I need to check the current folder first.
		//however if I am quick searching I need to know if the current folder contains a file/folder named .git, therefore in that case I need to process the subfolders first
		//so this call is conditioned on BeThorough being true while at the end of this function I have basically the same call conditionend to BeThorough being false
		TestPathForRepoAndParseIfExists(ri, newRepoSpec, false, true);
	}

	DIR* directoryPointer;
	//On success, readdir() returns a pointer to a dirent structure.  (This structure may be statically allocated; do not attempt to free(3) it.)
	const struct dirent* direntptr;
	directoryPointer = opendir(ri->DirectoryPath);
	if (directoryPointer != NULL)
	{
		while ((direntptr = readdir(directoryPointer)))
		{
			//printf("testing: %s (directory: %s)\n", direntptr->d_name, (direntptr->d_type == DT_DIR ? "TRUE" : "NO"));
			if (!BeThorough && Compare(direntptr->d_name, ".git")) {
				ri->isGit = true;
			}
			if (direntptr->d_type != DT_DIR || Compare(direntptr->d_name, ".") || Compare(direntptr->d_name, "..")) {
				//if the current file isn't a directory I needn't check for subdirectories
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
		fprintf(stderr, "failed on directory: %s\n", directoryName);
		perror("Couldn't open the directory");
	}
	if (!BeThorough) {
		//for an explanation to this call see the top of this function where another TestPathForRepoAndParseIfExists call exists
		TestPathForRepoAndParseIfExists(ri, newRepoSpec, false, false);
	}
	return ri;
}

void printTree_internal(RepoInfo* ri, const char* parentPrefix, bool anotherSameLevelEntryFollows, bool fullOut) {
	if (ri == NULL) {
		return;
	}
	if (ri->ParentDirectory != NULL) {
		printf("%s%s\u2500\u2500 ", parentPrefix, (anotherSameLevelEntryFollows ? "\u251c" : "\u2514"));
	}
	if (ri->isGit) {
		printf("%s", ri->DirectoryName);
		if (CONFIG_GIT_REPOTYPE) {
			printf(" [" COLOUR_GIT_BARE "%s" COLOUR_GIT_INDICATOR "GIT", ri->isBare ? "BARE " : "");
			if (ri->isSubModule) {
				printf("-SM" COLOUR_CLEAR);
				if (CONFIG_GIT_REPOTYPE_PARENT) {
					printf("@" COLOUR_GIT_PARENT "%s", ri->parentRepo);
				}
			}
			printf(COLOUR_CLEAR "]");
		}
		if (CONFIG_GIT_REPONAME) {
			printf(" " COLOUR_GIT_NAME "%s" COLOUR_CLEAR, ri->RepositoryName);
		}
		if (CONFIG_GIT_BRANCHNAME) {
			printf(" on " COLOUR_GIT_BRANCH "%s", ri->branch);
			if (CONFIG_GIT_BRANCH_OVERVIEW) {
				printf(COLOUR_GREYOUT "/%i+%i",
					ri->CountActiveBranches,
					ri->CountFullyMergedBranches);
			}
		}
		if (!ri->isBare && CONFIG_GIT_BRANCHSTATUS) {
			char* gitBranchInfo = ConstructGitBranchInfoString(ri);
			printf("%s%s%s",
				(CONFIG_GIT_BRANCHNAME || CONFIG_GIT_REPONAME) ? ":" : "",
				(CONFIG_GIT_BRANCH_OVERVIEW) ? "" : COLOUR_GREYOUT,
				gitBranchInfo);
			free(gitBranchInfo);
			gitBranchInfo = NULL;
		}

		if (CONFIG_GIT_REMOTE || ri->RepositoryOriginID_PREVIOUS != -1) {
			printf(" " COLOUR_CLEAR "from ");
		}

		char* GitComStrTemp = ConstructCommitStatusString(ri);
		char* GitStatStrTemp = ConstructGitStatusString(ri);
		//differentiate between display only and display after change
		if (ri->RepositoryOriginID_PREVIOUS != -1) {
			printf(COLOUR_GIT_ORIGIN "[%s(%i) -> %s(%i)]" COLOUR_CLEAR, NAMES[ri->RepositoryOriginID_PREVIOUS], ri->RepositoryOriginID_PREVIOUS, NAMES[ri->RepositoryOriginID], ri->RepositoryOriginID);
			if (!ri->isBare) {
				if (CONFIG_GIT_COMMIT_OVERVIEW) {
					printf("%s", GitComStrTemp);
				}
				if (CONFIG_GIT_LOCALCHANGES) {
					printf("%s", GitStatStrTemp);
				}
			}
			if (fullOut) {
				printf(COLOUR_GREYOUT " (%s -> %s)" COLOUR_CLEAR, ri->RepositoryUnprocessedOrigin_PREVIOUS, ri->RepositoryUnprocessedOrigin);
			}
			putc('\n', stdout);
		}
		else {
			if (CONFIG_GIT_REMOTE) {
				printf(COLOUR_GIT_ORIGIN "%s" COLOUR_CLEAR, ri->RepositoryDisplayedOrigin);
			}
			if (!ri->isBare) {
				if (CONFIG_GIT_COMMIT_OVERVIEW) {
					printf("%s", GitComStrTemp);
				}
				if (CONFIG_GIT_LOCALCHANGES) {
					printf("%s", GitStatStrTemp);
				}
			}
			if (fullOut && CONFIG_GIT_REMOTE) {
				printf(COLOUR_GREYOUT " (%s)" COLOUR_CLEAR, ri->RepositoryUnprocessedOrigin);
			}
			putc('\n', stdout);
		}
		free(GitStatStrTemp);
		free(GitComStrTemp);
		GitStatStrTemp = NULL;
		GitComStrTemp = NULL;

	}
	else {
		printf(COLOUR_GREYOUT "%s" COLOUR_CLEAR "\n", ri->DirectoryName);
		//this prints the name of intermediate folders that are not git repos, but contain a repo somewhere within
		//-> those are less important->print greyed out
	}
	fflush(stdout);
	RepoList* current = ri->SubDirectories;
	int procedSubDirs = 0;
	char* temp;
	if (asprintf(&temp, "%s%s", parentPrefix, (ri->ParentDirectory != NULL) ? (anotherSameLevelEntryFollows ? "\u2502   " : "    ") : "") == -1) ABORT_NO_MEMORY;
	while (current != NULL)
	{
		procedSubDirs++;
		printTree_internal(current->self, temp, procedSubDirs < ri->SubDirectoryCount, fullOut);
		current = current->next;
	}
	free(temp);
}

void printTree(RepoInfo* ri, bool Detailed) {
	printTree_internal(ri, "", ri->SubDirectoryCount > 1, Detailed);
}


void Cleanup() {
	for (int i = 0;i < MaxLocations;i++) {
		if (LOCS[i] != NULL) { free(LOCS[i]); };
		if (NAMES[i] != NULL) { free(NAMES[i]); };
		if (GitHubs[i] != NULL) { free(GitHubs[i]); };
	}
}

void DoSetup() {
	for (int i = 0;i < MaxLocations;i++) {
		LOCS[i] = NULL;
		NAMES[i] = NULL;
		GitHubs[i] = NULL;
	}
	numLOCS = 0;
	numGitHubs = 0;
	errno = 0;

	int buf_max_len = 1024;
	char* buf = (char*)malloc(sizeof(char) * buf_max_len);
	if (buf == NULL) ABORT_NO_MEMORY;

	char* configFilePath;
	const char* fileName = "/config.cfg";
	const char* pointerIntoEnv = getenv("ST_CFG");
	configFilePath = (char*)malloc(strlen(pointerIntoEnv) + strlen(fileName) + 1); // to account for NULL terminator
	if (configFilePath == NULL) ABORT_NO_MEMORY;
	strcpy(configFilePath, pointerIntoEnv);
	strcat(configFilePath, fileName);
	FILE* fp = fopen(configFilePath, "r");//open for read, will fail if configFilePath doesn't exist
	if (fp == NULL) {
		printf("config file didn't exist (%i: %s)\n", errno, strerror(errno));
		errno = 0;
		fp = fopen(configFilePath, "w+");//open for read/write -> create if not exists, then fill defaults, then read
		if (fp == NULL) {
			fprintf(stderr, "couldn't create file (%i: %s)\n", errno, strerror(errno));
			free(configFilePath);
			free(buf);
			return;
		}
		else {
			fprintf(fp, "###\n###THIS FILE IS NOT AUTOMATICALLY UPDATED AFTER INITIAL CREATION\n###CHECK THE TEMPLATE FILE AT $ST_SRC/DEFAULTCONFIG.cfg FOR POSSIBLE NEW OPTIONS\n###\n");
			//Create (or rather copy) default file
			const char* defaultConfigFileRelativePath = "/DEFAULTCONFIG.cfg";
			const char* defaultConfigFileDir = getenv("ST_SRC");
			char* defaultConfigFileFullPath;
			if (asprintf(&defaultConfigFileFullPath, "%s%s", defaultConfigFileDir, defaultConfigFileRelativePath) == -1) ABORT_NO_MEMORY;
			FILE* dfp = fopen(defaultConfigFileFullPath, "r");
			if (dfp != NULL) {
				while (fgets(buf, buf_max_len - 1, dfp) != NULL) {
					if (!StartsWith(buf, "###")) {
						//a line starting with ### is the "DO NOT EDIT" warning, skip that but copy everything else
						fprintf(fp, "%s", buf);
					}
				}

				printf("created default config file %s from %s\n", configFilePath, defaultConfigFileFullPath);
				fclose(dfp);
			}
			else {
				printf("WARNING: COULD NOT READ CONFIG FILE TEMPLATE %s (%i: %s)\nTHEREFORE CANNOT POPULATE DEFAULT CONFIG FILE %s\nCONFIG FILE AT %s WILL EXIST BUT BE EMPTY, PLEASE MANUALLY CHECK THE FILE", defaultConfigFileFullPath, errno, strerror(errno), configFilePath, configFilePath);
				free(configFilePath);
				free(buf);
				free(defaultConfigFileFullPath);
				fclose(fp);
				return;
			}
			fflush(fp);
			rewind(fp);
			free(defaultConfigFileFullPath);
			defaultConfigFileFullPath = NULL;
		}
	}
	free(configFilePath);

	bool UnknownConfig = false;
	//at this point I know for certain a config file does exist
	while (fgets(buf, buf_max_len - 1, fp) != NULL) {
		if (buf[0] == '#') {
			continue;
		}
		else {
			if (TerminateStrOn(buf, DEFAULT_TERMINATORS) == 0) {
				continue;
			}
			if (StartsWith(buf, "ORIGIN_ALIAS:	")) {
				if (numLOCS >= (MaxLocations - 1)) {
					fprintf(stderr, "WARNING: YOU HAVE CONFIGURED MORE THAN %1$i ORIGIN_ALIAS ENTRIES. ONLY THE FIRST %1$i WILL BE USED\n", MaxLocations);
					continue;
				}
				//found origin
				char* actbuf = buf + 14;//the 8 is the offset to just behind "ORIGIN:	"
				int i = 0;
				while (i < buf_max_len - 14 && actbuf[i] != '\t') {
					i++;
				}
				if (i < (buf_max_len - 14 - 1) && actbuf[i] == '\t') {
					if (asprintf(&LOCS[numLOCS], "%s", actbuf + i + 1) == -1) { fprintf(stderr, "WARNING: not enough memory, provisionally continuing, be prepared!"); }
					actbuf[i] = 0x00;
					if (asprintf(&NAMES[numLOCS], "%s", actbuf) == -1) { fprintf(stderr, "WARNING: not enough memory, provisionally continuing, be prepared!"); }
#ifdef DEBUG
					printf("origin>%s|%s<\n", NAMES[numLOCS], LOCS[numLOCS]);
#endif
					numLOCS++;
				}
			}
			else if (StartsWith(buf, "GITHUB_HOST:	")) {
				if (numGitHubs >= (MaxLocations - 1)) {
					fprintf(stderr, "WARNING: YOU HAVE CONFIGURED MORE THAN %1$i GITHUB_HOST ENTRIES. ONLY THE FIRST %1$i WILL BE USED\n", MaxLocations);
					continue;
				}
				//found host
				if (asprintf(&GitHubs[numGitHubs], "%s", buf + 13) == -1) { fprintf(stderr, "WARNING: not enough memory, provisionally continuing, be prepared!"); }
				//the +6 is the offset to just after "HOST:	"
#ifdef DEBUG
				printf("host>%s<\n", GitHubs[numGitHubs]);
#endif
				numGitHubs++;
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.QUICK.MAXBRANCHES:	")) {
				CONFIG_LSGIT_QUICK_BRANCHLIMIT = atoi(buf + 35);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 35, CONFIG_LSGIT_QUICK_BRANCHLIMIT);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.THOROUGH.MAXBRANCHES:	")) {
				CONFIG_LSGIT_THOROUGH_BRANCHLIMIT = atoi(buf + 38);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 38, CONFIG_LSGIT_THOROUGH_BRANCHLIMIT);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.WARN_BRANCH_LIMIT:	")) {
				CONFIG_LSGIT_WARN_BRANCHLIMIT = Compare("true", buf + 35);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 35, CONFIG_LSGIT_WARN_BRANCHLIMIT);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LOWPROMPT.PATH.LIMIT_DISPLAY_LENGTH.ENABLE:	")) {
				CONFIG_LOWPROMPT_PATH_LIMIT = Compare("true", buf + 54);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 54, CONFIG_LOWPROMPT_PATH_LIMIT);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LOWPROMPT.PATH.LIMIT_DISPLAY_LENGTH.TARGET:	")) {
				CONFIG_LOWPROMPT_PATH_MAXLEN = atoi(buf + 54);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 54, CONFIG_LOWPROMPT_PATH_MAXLEN);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LOWPROMPT.RETURNCODE.ENABLE:	")) {
				CONFIG_LOWPROMPT_RETCODE = Compare("true", buf + 39);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 39, CONFIG_LOWPROMPT_RETCODE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LOWPROMPT.RETURNCODE.DECODE.ENABLE:	")) {
				CONFIG_LOWPROMPT_RETCODE_DECODE = Compare("true", buf + 46);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 46, CONFIG_LOWPROMPT_RETCODE_DECODE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LOWPROMPT.COMMAND_TIMER.ENABLE:	")) {
				CONFIG_LOWPROMPT_TIMER = Compare("true", buf + 42);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 42, CONFIG_LOWPROMPT_TIMER);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.SSHINFO.ENABLE:	")) {
				CONFIG_PROMPT_SSH = Compare("true", buf + 33);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 33, CONFIG_PROMPT_SSH);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.TERMINALDEVICE.ENABLE:	")) {
				CONFIG_PROMPT_TERMINAL_DEVICE = Compare("true", buf + 40);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 40, CONFIG_PROMPT_TERMINAL_DEVICE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.TIME.ENABLE:	")) {
				CONFIG_PROMPT_TIME = Compare("true", buf + 30);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 30, CONFIG_PROMPT_TIME);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.TIMEZONE.ENABLE:	")) {
				CONFIG_PROMPT_TIMEZONE = Compare("true", buf + 34);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 34, CONFIG_PROMPT_TIMEZONE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.DATE.ENABLE:	")) {
				CONFIG_PROMPT_DATE = Compare("true", buf + 30);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 30, CONFIG_PROMPT_DATE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.CALENDARWEEK.ENABLE:	")) {
				CONFIG_PROMPT_CALENDARWEEK = Compare("true", buf + 38);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 38, CONFIG_PROMPT_CALENDARWEEK);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.PROXYSTATUS.ENABLE:	")) {
				CONFIG_PROMPT_PROXY = Compare("true", buf + 37);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 37, CONFIG_PROMPT_PROXY);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.NETWORK.ENABLE:	")) {
				CONFIG_PROMPT_NETWORK = Compare("true", buf + 33);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 33, CONFIG_PROMPT_NETWORK);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.BACKGROUNDJOBS.ENABLE:	")) {
				CONFIG_PROMPT_JOBS = Compare("true", buf + 40);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 40, CONFIG_PROMPT_JOBS);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.POWER.ENABLE:	")) {
				CONFIG_PROMPT_POWER = Compare("true", buf + 31);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 31, CONFIG_PROMPT_POWER);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.ENABLE:	")) {
				CONFIG_PROMPT_OVERALL_ENABLE = Compare("true", buf + 25);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 25, CONFIG_PROMPT_OVERALL_ENABLE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.GIT.ENABLE:	")) {
				CONFIG_PROMPT_GIT = Compare("true", buf + 29);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 29, CONFIG_PROMPT_GIT);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.NETWORK.INTERFACES.DEFAULT.ENABLE:	")) {
				CONFIG_PROMPT_NET_IFACE = Compare("true", buf + 52);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 52, CONFIG_PROMPT_NET_IFACE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.NETWORK.INTERFACES.NONDEFAULT.ENABLE:	")) {
				CONFIG_PROMPT_NET_ADDITIONAL = Compare("true", buf + 55);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 55, CONFIG_PROMPT_NET_ADDITIONAL);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.NETWORK.ROUTINGINFO.ENABLE:	")) {
				CONFIG_PROMPT_NET_ROUTE = Compare("true", buf + 45);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 45, CONFIG_PROMPT_NET_ROUTE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.NETWORK.LINKSPEED.ENABLE:	")) {
				CONFIG_PROMPT_NET_LINKSPEED = Compare("true", buf + 43);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 43, CONFIG_PROMPT_NET_LINKSPEED);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.GIT.REPOTYPE.ENABLE:	")) {
				CONFIG_PROMPT_GIT_REPOTYPE = Compare("true", buf + 38);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 38, CONFIG_PROMPT_GIT_REPOTYPE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.GIT.REPOTYPE.PARENT_REPO.ENABLE:	")) {
				CONFIG_PROMPT_GIT_REPOTYPE_PARENT = Compare("true", buf + 50);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 50, CONFIG_PROMPT_GIT_REPOTYPE_PARENT);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.GIT.COMMIT_OVERVIEW.ENABLE:	")) {
				CONFIG_PROMPT_GIT_COMMITS = Compare("true", buf + 45);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 45, CONFIG_PROMPT_GIT_COMMITS);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.QUICK.REPOTYPE.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_REPOTYPE = Compare("true", buf + 39);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 39, CONFIG_LSGIT_QUICK_REPOTYPE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.QUICK.REPOTYPE.PARENT_REPO.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_REPOTYPE_PARENT = Compare("true", buf + 51);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 51, CONFIG_LSGIT_QUICK_REPOTYPE_PARENT);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.QUICK.REPONAME.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_REPONAME = Compare("true", buf + 39);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 39, CONFIG_LSGIT_QUICK_REPONAME);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.QUICK.BRANCH.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_BRANCHNAME = Compare("true", buf + 37);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 37, CONFIG_LSGIT_QUICK_BRANCHNAME);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.QUICK.BRANCH.OVERVIEW.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_BRANCHINFO = Compare("true", buf + 46);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 46, CONFIG_LSGIT_QUICK_BRANCHINFO);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.QUICK.REMOTE.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_REMOTE = Compare("true", buf + 37);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 37, CONFIG_LSGIT_QUICK_REMOTE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.QUICK.COMMIT_OVERVIEW.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_COMMITS = Compare("true", buf + 46);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 46, CONFIG_LSGIT_QUICK_COMMITS);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.QUICK.LOCALCHANGES.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_GITSTATUS = Compare("true", buf + 43);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 43, CONFIG_LSGIT_QUICK_GITSTATUS);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.THOROUGH.REPOTYPE.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_REPOTYPE = Compare("true", buf + 42);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 42, CONFIG_LSGIT_THOROUGH_REPOTYPE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.THOROUGH.REPOTYPE.PARENT_REPO.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_REPOTYPE_PARENT = Compare("true", buf + 54);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 54, CONFIG_LSGIT_THOROUGH_REPOTYPE_PARENT);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.THOROUGH.REPONAME.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_REPONAME = Compare("true", buf + 42);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 42, CONFIG_LSGIT_THOROUGH_REPONAME);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.THOROUGH.BRANCH.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_BRANCHNAME = Compare("true", buf + 40);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 40, CONFIG_LSGIT_THOROUGH_BRANCHNAME);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.THOROUGH.BRANCH.OVERVIEW.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_BRANCHINFO = Compare("true", buf + 49);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 49, CONFIG_LSGIT_THOROUGH_BRANCHINFO);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.THOROUGH.REMOTE.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_REMOTE = Compare("true", buf + 40);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 40, CONFIG_LSGIT_THOROUGH_REMOTE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.THOROUGH.COMMIT_OVERVIEW.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_COMMITS = Compare("true", buf + 49);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 49, CONFIG_LSGIT_THOROUGH_COMMITS);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.THOROUGH.LOCALCHANGES.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_GITSTATUS = Compare("true", buf + 46);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 46, CONFIG_LSGIT_THOROUGH_GITSTATUS);
#endif
			}

			else if (StartsWith(buf, "REPOTOOLS.PROMPT.GIT.REMOTE.ENABLE:	")) {
				CONFIG_PROMPT_GIT_REMOTE = Compare("true", buf + 36);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 36, CONFIG_PROMPT_GIT_REMOTE);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.GIT.BRANCH.OVERVIEW.ENABLE:	")) {
				CONFIG_PROMPT_GIT_BRANCHINFO = Compare("true", buf + 45);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 45, CONFIG_PROMPT_GIT_BRANCHINFO);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.GIT.BRANCHSTATUS.ENABLE:	")) {
				CONFIG_PROMPT_GIT_BRANCHSTATUS = Compare("true", buf + 42);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 42, CONFIG_PROMPT_GIT_BRANCHSTATUS);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.QUICK.BRANCHSTATUS.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_BRANCHSTATUS = Compare("true", buf + 43);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 43, CONFIG_LSGIT_QUICK_BRANCHSTATUS);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.LSGIT.THOROUGH.BRANCHSTATUS.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_BRANCHSTATUS = Compare("true", buf + 46);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 46, CONFIG_LSGIT_THOROUGH_BRANCHSTATUS);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.GIT.BRANCH.ENABLE:	")) {
				CONFIG_PROMPT_GIT_BRANCHNAME = Compare("true", buf + 36);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 36, CONFIG_PROMPT_GIT_BRANCHNAME);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.GIT.REPONAME.ENABLE:	")) {
				CONFIG_PROMPT_GIT_REPONAME = Compare("true", buf + 38);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 38, CONFIG_PROMPT_GIT_REPONAME);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.GIT.LOCALCHANGES.ENABLE:	")) {
				CONFIG_PROMPT_GIT_GITSTATUS = Compare("true", buf + 42);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 42, CONFIG_PROMPT_GIT_GITSTATUS);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.GIT.MAXBRANCHES:	")) {
				CONFIG_PROMPT_GIT_BRANCHLIMIT = atoi(buf + 34);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 34, CONFIG_PROMPT_GIT_BRANCHLIMIT);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.GIT.WARN_BRANCH_LIMIT:	")) {
				CONFIG_LSGIT_WARN_BRANCHLIMIT = Compare("true", buf + 40);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 40, CONFIG_LSGIT_WARN_BRANCHLIMIT);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.USER.ENABLE:	")) {
				CONFIG_PROMPT_USER = Compare("true", buf + 30);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 30, CONFIG_PROMPT_USER);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.HOST.ENABLE:	")) {
				CONFIG_PROMPT_HOST = Compare("true", buf + 30);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 30, CONFIG_PROMPT_HOST);
#endif
			}
			else if (StartsWith(buf, "REPOTOOLS.PROMPT.FILLER_CHAR:	")) {
				CONFIG_PROMPT_FILLER_CHAR = buf[31];
#ifdef DEBUG
				printf("CONFIG:%s : %s -> '%c'\n", buf, buf + 30, CONFIG_PROMPT_FILLER_CHAR);
#endif
			}
			else {
				fprintf(stderr, "Warning: unknown entry in config file: >%s<\n", buf);
				UnknownConfig = true;
			}
		}
	}
	fclose(fp);
	free(buf);
	if (UnknownConfig) {
		fprintf(stderr, "WARNING: You have unknown entires in your config file (%s/config.cfg).\n\tPlease check the template at %s/DEFAULTCONFIG.cfg for a list of all understood options and correct your own config file\n", getenv("ST_CFG"), getenv("ST_SRC"));
	}
}

void ListAvailableRemotes() {
	for (int i = 0;i < numLOCS;i++) {
		printf(COLOUR_GIT_ORIGIN "%s" COLOUR_CLEAR " (-> %s)\n", NAMES[i], LOCS[i]);
	}
}

typedef struct {
	char* ipv4;
	uint32_t metric;
	bool isDefault;
	uint8_t IPV4cidr;
	char* device;
	char* linkspeed;
	char* routedNet; //not used if isDefault is true
} NetDevice;

typedef struct NetList_t NetList;
struct NetList_t {
	NetDevice dev;
	NetList* next;
	NetList* prev;
};

NetList* InitNetListElement() {
	NetList* a = (NetList*)malloc(sizeof(NetList));
	if (a == NULL) ABORT_NO_MEMORY;
	NetDevice* e = &(a->dev);
	e->ipv4 = NULL;
	e->metric = UINT32_MAX;
	e->isDefault = false;
	e->IPV4cidr = 0;
	e->device = NULL;
	e->linkspeed = NULL;
	e->routedNet = NULL;
	return a;
}

NetList* InsertIntoNetListSorted(NetList* head, const char* device, const char* ipv4, int metric, bool isDefault, int IPV4cidr, const char* linkspeed, const char* routedNet) {
	if (head != NULL && head->dev.isDefault == false && isDefault == true) {
		abortMessage("assumption on the order of routes incorrect (assume default routes are listed first)");
	}
	if (head == NULL) {
		//Create Initial Element (List didn't exist previously)
		NetList* n = InitNetListElement();
		n->next = NULL;
		n->prev = NULL;
		//IP and device are the minimum set necessary (this was the original assumption, sincee reduced to just device)
		//originally I asserted device AND IP must be present, but on my linode VM in the entry denoting the default route there is no IP -> that assertion does not hold
		assert(device != NULL);
		if (asprintf(&(n->dev.device), "%s", device) == -1) ABORT_NO_MEMORY;
		if (ipv4 != NULL) {
			if (n->dev.ipv4 != NULL) { free(n->dev.ipv4); }
			if (asprintf(&(n->dev.ipv4), "%s", ipv4) == -1) ABORT_NO_MEMORY;
		}
		n->dev.metric = metric;
		n->dev.isDefault = isDefault;
		if (IPV4cidr != 0) n->dev.IPV4cidr = IPV4cidr;
		if (linkspeed != NULL) {
			if (n->dev.linkspeed != NULL) { free(n->dev.linkspeed); }
			if (asprintf(&(n->dev.linkspeed), "%s", linkspeed) == -1) ABORT_NO_MEMORY;
		}
		if (routedNet != NULL) {
			if (n->dev.routedNet != NULL) { free(n->dev.routedNet); }
			if (asprintf(&(n->dev.routedNet), "%s", routedNet) == -1) ABORT_NO_MEMORY;
		}

		return n;
	}
	else if (Compare(head->dev.device, device)) {
		//device name matches, this is just additional info
		head->dev.metric = metric;
		if (IPV4cidr != 0) head->dev.IPV4cidr = IPV4cidr;
		head->dev.isDefault = head->dev.isDefault || isDefault;
		if (linkspeed != NULL) {
			if (head->dev.linkspeed != NULL) { free(head->dev.linkspeed); }
			if (asprintf(&(head->dev.linkspeed), "%s", linkspeed) == -1) ABORT_NO_MEMORY;
		}
		if (routedNet != NULL) {
			if (head->dev.routedNet != NULL) { free(head->dev.routedNet); }
			if (asprintf(&(head->dev.routedNet), "%s", routedNet) == -1) ABORT_NO_MEMORY;
		}
		if (ipv4 != NULL) {
			if (head->dev.ipv4 != NULL) { free(head->dev.ipv4); }
			if (asprintf(&(head->dev.ipv4), "%s", ipv4) == -1) ABORT_NO_MEMORY;
		}
		return head;
	}
	else if (metric < head->dev.metric) {
		//The new Element has a lower metric -> insert before myslef
		NetList* n = InitNetListElement();
		n->next = head;
		n->prev = head->prev;
		if (head->prev != NULL) head->prev->next = n;
		head->prev = n;
		//IP and device are the minimum set necessary (this was the original assumption, sincee reduced to just device)
		//originally I asserted device AND IP must be present, but on my linode VM in the entry denoting the default route there is no IP -> that assertion does not hold
		assert(device != NULL);
		if (asprintf(&(n->dev.device), "%s", device) == -1) ABORT_NO_MEMORY;
		if (ipv4 != NULL) {
			if (n->dev.ipv4 != NULL) { free(n->dev.ipv4); }
			if (asprintf(&(n->dev.ipv4), "%s", ipv4) == -1) ABORT_NO_MEMORY;
		}
		n->dev.metric = metric;
		n->dev.isDefault = isDefault;
		if (IPV4cidr != 0) n->dev.IPV4cidr = IPV4cidr;
		if (linkspeed != NULL) {
			if (n->dev.linkspeed != NULL) { free(n->dev.linkspeed); }
			if (asprintf(&(head->dev.linkspeed), "%s", linkspeed) == -1) ABORT_NO_MEMORY;
		}
		if (routedNet != NULL) {
			if (n->dev.routedNet != NULL) { free(n->dev.routedNet); }
			if (asprintf(&(n->dev.routedNet), "%s", routedNet) == -1) ABORT_NO_MEMORY;
		}
		return n;
	}
	else if (head->next == NULL) {
		//The New Element is NOT Equal to myself and is NOT alphabetically before myself (as per the earlier checks)
		//on top of that, I AM the LAST element, so I can simply create a new last element
		NetList* n = InitNetListElement();
		n->next = NULL;
		n->prev = head;

		head->next = n;
		//IP and device are the minimum set necessary (this was the original assumption, sincee reduced to just device)
		//originally I asserted device AND IP must be present, but on my linode VM in the entry denoting the default route there is no IP -> that assertion does not hold
		assert(device != NULL);
		if (asprintf(&(n->dev.device), "%s", device) == -1) ABORT_NO_MEMORY;
		if (ipv4 != NULL) {
			if (n->dev.ipv4 != NULL) { free(n->dev.ipv4); }
			if (asprintf(&(n->dev.ipv4), "%s", ipv4) == -1) ABORT_NO_MEMORY;
		}
		n->dev.metric = metric;
		n->dev.isDefault = isDefault;
		if (IPV4cidr != 0) n->dev.IPV4cidr = IPV4cidr;
		if (linkspeed != NULL) {
			if (n->dev.linkspeed != NULL) { free(n->dev.linkspeed); }
			if (asprintf(&(head->dev.linkspeed), "%s", linkspeed) == -1) ABORT_NO_MEMORY;
		}
		if (routedNet != NULL) {
			if (n->dev.routedNet != NULL) { free(n->dev.routedNet); }
			if (asprintf(&(n->dev.routedNet), "%s", routedNet) == -1) ABORT_NO_MEMORY;
		}

		return head;
	}
	else {
		//The New Element is NOT Equal to myslef and is NOT alphabetically before myself (as per the earlier checks)
		//This time there IS another element after myself -> defer to it.
		head->next = InsertIntoNetListSorted(head->next, device, ipv4, metric, isDefault, IPV4cidr, linkspeed, routedNet);
		return head;
	}
}

char* GetIfaceSpeed(const char* iface) {
	char* ret;
	int size = 32;
	char* result = (char*)malloc(sizeof(char) * size);
	if (result == NULL) ABORT_NO_MEMORY;
	char* command;
	if (asprintf(&command, "nmcli -g CAPABILITIES.SPEED device show %s 2>/dev/null | sed -E 's~([0-9]+) (.).+~\\1\\2~'", iface) == -1) ABORT_NO_MEMORY;
	FILE* fp = popen(command, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "failed running process %s\n", command);
	}
	else {
		while (fgets(result, size - 1, fp) != NULL)
		{
			TerminateStrOn(result, DEFAULT_TERMINATORS);
			if (Compare(result, ""))continue;
			if (Compare(result, "1000M")) {
				if (asprintf(&ret, "1G") == -1) ABORT_NO_MEMORY;
			}
			else if (Compare(result, "2500M")) {
				if (asprintf(&ret, "2.5G") == -1) ABORT_NO_MEMORY;
			}
			else if (Compare(result, "5000M")) {
				if (asprintf(&ret, "5G") == -1) ABORT_NO_MEMORY;
			}
			else if (Compare(result, "10000M")) {
				if (asprintf(&ret, "10G") == -1) ABORT_NO_MEMORY;
			}
			else if (Compare(result, "unknown")) {
				return NULL;
			}
			else {
				if (asprintf(&ret, "%s", result) == -1) ABORT_NO_MEMORY;
			}
			return ret;
		}
	}
	return NULL;
}

typedef struct {
	char* BasicIPInfo;
	char* AdditionalIPInfo;
	char* RouteInfo;
} IpTransportStruct;

IpTransportStruct GetBaseIPString() {
	IpTransportStruct ret;
	ret.BasicIPInfo = NULL;
	ret.AdditionalIPInfo = NULL;
	ret.RouteInfo = NULL;
	if (!CONFIG_PROMPT_NETWORK) {
		//network is completely configured out -> do not do anything
		ret.BasicIPInfo = malloc(sizeof(char) * 1);
		if (ret.BasicIPInfo == NULL) ABORT_NO_MEMORY;
		ret.AdditionalIPInfo = malloc(sizeof(char) * 1);
		if (ret.AdditionalIPInfo == NULL) ABORT_NO_MEMORY;
		ret.RouteInfo = malloc(sizeof(char) * 1);
		if (ret.RouteInfo == NULL) ABORT_NO_MEMORY;
		ret.BasicIPInfo[0] = 0x00;
		ret.AdditionalIPInfo[0] = 0x00;
		ret.RouteInfo[0] = 0x00;
		return ret;
	}
	uint8_t RouteRegexGroupCount = 16;
	regmatch_t RouteRegexGroups[RouteRegexGroupCount];
	regex_t RouteRegex;
	const char* RouteRegexString = "^(((default) via ([0-9.]+))|(([0-9.]+)/([0-9]+))) dev ([^ ]+)( proto [^ ]+)?( scope link)?( src ([0-9.]+))?( metric ([0-9]+))?( linkdown)? *$";
	//the following was the originally used regex. it doesn't work for some network configurations (such as linode/akamai VPS).
	//const char* RouteRegexString="^(((default) via ([0-9.]+))|(([0-9.]+)/([0-9]+))).*?dev ([^ ]+).*?src ([0-9.]+)( metric ([0-9]+))?( linkdown)?.*$";
	//root cause is problems with lazy matching, which C's regex engine can't do.
	//I haed copilot help in creating a "fixed" regex. the output was this:
	//^((default[[:space:]]+via[[:space:]]+([0-9.]+))|(([0-9.]+)/([0-9]+)))[[:space:]]+dev[[:space:]]+([^[:space:]]+)(?:[[:space:]]+proto[[:space:]]+[^[:space:]]+)?(?:[[:space:]]+scope[[:space:]]+link)?(?:[[:space:]]+src[[:space:]]+([0-9.]+))?(?:[[:space:]]+metric[[:space:]]+([0-9]+))?(?:[[:space:]]+linkdown)?[[:space:]]*$
	//I have then refined it into a much smaller and more easily readable version which is in use currently
#define RouteIsDefaultIndex 3
#define RouteNextHopIndex 4
#define RouteRoutedNetIndex 6
#define RouteCidrIndex 7
#define RouteDeviceIndex 8
#define RouteIpIndex 12
#define RouteMetricIndex 14
#define RouteLinkDownIndex 15
	int IpRegexReturnCode;
	IpRegexReturnCode = regcomp(&RouteRegex, RouteRegexString, REG_EXTENDED | REG_NEWLINE);
	if (IpRegexReturnCode)
	{
		char* regErrorBuf = (char*)malloc(sizeof(char) * 1024);
		if (regErrorBuf == NULL) ABORT_NO_MEMORY;
		regerror(IpRegexReturnCode, &RouteRegex, regErrorBuf, 1024);
		printf("Could not compile regular expression '%s'. [%i(%s)]\n", RouteRegexString, IpRegexReturnCode, regErrorBuf);
		fflush(stdout);
		free(regErrorBuf);
		exit(1);
	};


	NetList* head = NULL;

	int size = 1024;
	char* result = (char*)malloc(sizeof(char) * size);
	if (result == NULL) ABORT_NO_MEMORY;
	const char* command = "ip route show";
	FILE* fp = popen(command, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "failed running process %s\n", command);
		fflush(stderr);
	}
	else {
		while (fgets(result, size - 1, fp) != NULL)
		{
			TerminateStrOn(result, DEFAULT_TERMINATORS);
			//fprintf(stderr, "\n\nresult: %s\n", result);
			IpRegexReturnCode = regexec(&RouteRegex, result, RouteRegexGroupCount, RouteRegexGroups, 0);
			//man regex (3): regexec() returns zero for a successful match or REG_NOMATCH for failure.
			if (IpRegexReturnCode == 0)
			{
				if (RouteRegexGroups[RouteLinkDownIndex].rm_eo - RouteRegexGroups[RouteLinkDownIndex].rm_so != 0) {
					continue; //linkdown matched, therefore ignore this interface
				}
				char* device = NULL;
				char* ipv4 = NULL;
				uint32_t metric = UINT32_MAX;
				bool isDefault = false;
				int IPV4cidr = 0;
				char* linkspeed = NULL;
				char* routednet = NULL;
				int len;
				len = RouteRegexGroups[RouteDeviceIndex].rm_eo - RouteRegexGroups[RouteDeviceIndex].rm_so;
				if (len > 0) {
					device = malloc(sizeof(char) * (len + 1));
					if (device == NULL) ABORT_NO_MEMORY;
					strncpy(device, result + RouteRegexGroups[RouteDeviceIndex].rm_so, len);
					device[len] = 0x00;
				}
				if (RouteRegexGroups[RouteIsDefaultIndex].rm_eo - RouteRegexGroups[RouteIsDefaultIndex].rm_so != 0) {
					isDefault = true;
				}
				else if (CONFIG_PROMPT_NET_LINKSPEED) {
					linkspeed = GetIfaceSpeed(device);
				}
				len = RouteRegexGroups[RouteIpIndex].rm_eo - RouteRegexGroups[RouteIpIndex].rm_so;
				if (len > 0) {
					ipv4 = malloc(sizeof(char) * (len + 1));
					if (ipv4 == NULL) ABORT_NO_MEMORY;
					strncpy(ipv4, result + RouteRegexGroups[RouteIpIndex].rm_so, len);
					ipv4[len] = 0x00;
				}
				len = RouteRegexGroups[RouteMetricIndex].rm_eo - RouteRegexGroups[RouteMetricIndex].rm_so;
				if (len > 0) {
					char* temp = malloc(sizeof(char) * (len + 1));
					if (temp == NULL) ABORT_NO_MEMORY;
					strncpy(temp, result + RouteRegexGroups[RouteMetricIndex].rm_so, len);
					temp[len] = 0x00;
					metric = atoi(temp);
					free(temp);
					temp = NULL;
				}
				len = RouteRegexGroups[RouteCidrIndex].rm_eo - RouteRegexGroups[RouteCidrIndex].rm_so;
				if (len > 0) {
					char* temp = malloc(sizeof(char) * (len + 1));
					if (temp == NULL) ABORT_NO_MEMORY;
					strncpy(temp, result + RouteRegexGroups[RouteCidrIndex].rm_so, len);
					temp[len] = 0x00;
					IPV4cidr = atoi(temp);
					free(temp);
					temp = NULL;
				}
				len = RouteRegexGroups[RouteRoutedNetIndex].rm_eo - RouteRegexGroups[RouteRoutedNetIndex].rm_so;
				if (len > 0) {
					routednet = malloc(sizeof(char) * (len + 1));
					if (routednet == NULL) ABORT_NO_MEMORY;
					strncpy(routednet, result + RouteRegexGroups[RouteRoutedNetIndex].rm_so, len);
					routednet[len] = 0x00;
				}
				head = InsertIntoNetListSorted(head, device, ipv4, metric, isDefault, IPV4cidr, linkspeed, routednet);
				if (device != NULL)free(device);
				if (ipv4 != NULL)free(ipv4);
				if (linkspeed != NULL)free(linkspeed);
				if (routednet != NULL)free(routednet);
			}
			else if (DIPFALSCHEISSER_WARNINGS) {
				printf("ERROR: IP regex match returned %i -> IP info is likely incomplete\n", IpRegexReturnCode);fflush(stdout);
			}
		}
		pclose(fp);
	}
	free(result);

	//res (the standard IP line should get (numDefaultRoutes+1)*(1+4+8+4+1+(4*(3+1))+4+4+4+4+4+16+4) bytes (space+CSI+device+CSI+:IP+CSI+CIDR+CSI+CSI+CSI+metric+CSI) bytes
	//the above calculation comes out to 74 bytes, so if I assign 80 bytes per default route I sould be fine.
	//for the third line (the actual route display) I think 6+(10*numNonDefaultRoutes)+30 should be enough

	NetList* current = head;
	int numDefaultRoutes = 0;
	int numNonDefaultRoutes = 0;
	uint32_t lowestMetric = UINT32_MAX;
	while (current != NULL) {
		if (current->dev.isDefault) {
			numDefaultRoutes++;
		}
		else {
			numNonDefaultRoutes++;
		}
		if (current->dev.metric < lowestMetric) {
			lowestMetric = current->dev.metric;
		}
		//fprintf(stderr, "%s:%s/%i@%i %i for %s\n", current->dev.device, current->dev.ipv4, current->dev.IPV4cidr, current->dev.metric, current->dev.isDefault, current->dev.routedNet);
		current = current->next;
	}
	//this works under the assumption the interface names aren't longer than 10-ish characters (true for enpXsY or eth1 or wlpXsY) however at some point my system decided to use wlx18d6c713d4ed as default device and completely ignore the actually fast wifi chip

	int basicIPStringLen = (CONFIG_PROMPT_NET_IFACE ? (2 + (112 * numDefaultRoutes)) : 1);
	ret.BasicIPInfo = malloc(sizeof(char) * basicIPStringLen);
	if (ret.BasicIPInfo == NULL) ABORT_NO_MEMORY;
	int nondefaultIpStringLen = (CONFIG_PROMPT_NET_ADDITIONAL ? (2 + (numNonDefaultRoutes * 96)) : 1);
	ret.AdditionalIPInfo = malloc(sizeof(char) * nondefaultIpStringLen);
	if (ret.AdditionalIPInfo == NULL) ABORT_NO_MEMORY;
	int rounteinfoLen = (CONFIG_PROMPT_NET_ROUTE ? (48 + (16 * numDefaultRoutes)) : 1);
	ret.RouteInfo = malloc(sizeof(char) * rounteinfoLen);
	if (ret.RouteInfo == NULL) ABORT_NO_MEMORY;

	int baseIPlenUsed = 0;
	int nondefaultLenUsed = 0;
	int routeinfoLenUsed = 0;


	current = head;
	for (int i = 0;i < numDefaultRoutes;i++) {
		if (CONFIG_PROMPT_NET_IFACE) {
			baseIPlenUsed += snprintf(ret.BasicIPInfo + baseIPlenUsed, basicIPStringLen - (baseIPlenUsed + 1), " %s%s\e[0m:%s", (current->dev.metric == lowestMetric && numDefaultRoutes > 1 ? "\e[4m" : ""), current->dev.device, current->dev.ipv4);
			if (current->dev.IPV4cidr > 0) {
				baseIPlenUsed += snprintf(ret.BasicIPInfo + baseIPlenUsed, basicIPStringLen - (baseIPlenUsed + 1), "\e[38;5;244m/%i\e[0m", current->dev.IPV4cidr);
			}
			if (numDefaultRoutes > 1) {
				baseIPlenUsed += snprintf(ret.BasicIPInfo + baseIPlenUsed, basicIPStringLen - (baseIPlenUsed + 1), "\e[38;5;240m\e[2m\e[3m~%u\e[0m", current->dev.metric);
			}
			if (current->dev.linkspeed != NULL) {
				baseIPlenUsed += snprintf(ret.BasicIPInfo + baseIPlenUsed, basicIPStringLen - (baseIPlenUsed + 1), "\e[38;5;238m\e[2m@%s\e[0m", current->dev.linkspeed);
			}
		}
		current = current->next;
	}

	//I intentionally didn't reset current
	if (numNonDefaultRoutes > 0 && CONFIG_PROMPT_NET_ADDITIONAL) {
		for (int i = 0;i < numNonDefaultRoutes;i++) {
			nondefaultLenUsed += snprintf(ret.AdditionalIPInfo + nondefaultLenUsed, nondefaultIpStringLen - (nondefaultLenUsed + 1), " \e[38;5;244m\e[3m%s:%s\e[0m", current->dev.device, current->dev.ipv4);
			if (current->dev.IPV4cidr > 0) {
				nondefaultLenUsed += snprintf(ret.AdditionalIPInfo + nondefaultLenUsed, nondefaultIpStringLen - (nondefaultLenUsed + 1), "\e[38;5;240m\e[2m\e[3m/%i\e[0m", current->dev.IPV4cidr);
			}
			current = current->next;
		}
	}

	if (numNonDefaultRoutes > 0 && CONFIG_PROMPT_NET_ROUTE) {
		current = head;
		routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), " \e[38;5;244m\e[2m*->");
		if (numDefaultRoutes > 1) {
			routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), "{");
		}
		routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), "%s", current->dev.device);
		current = current->next;
		for (int i = 1;i < numDefaultRoutes;i++) {
			routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), ",%s", current->dev.device);
			current = current->next;
		}
		if (numDefaultRoutes > 1) {
			routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), "}");
		}
		if (numNonDefaultRoutes > 1) {
			routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), "  %i additional routes\e[0m", numNonDefaultRoutes);
		}
		else {
			routeinfoLenUsed += snprintf(ret.RouteInfo + routeinfoLenUsed, rounteinfoLen - (routeinfoLenUsed + 1), " %s/%i->%s\e[0m", current->dev.routedNet, current->dev.IPV4cidr, current->dev.device);
		}
	}
	assert(baseIPlenUsed < basicIPStringLen);
	assert(nondefaultLenUsed < nondefaultIpStringLen);
	assert(routeinfoLenUsed < rounteinfoLen);
	ret.BasicIPInfo[baseIPlenUsed] = 0x00;
	ret.AdditionalIPInfo[nondefaultLenUsed] = 0x00;
	ret.RouteInfo[routeinfoLenUsed] = 0x00;
#ifdef DEBUG
	fprintf(stderr, ">%s< (%i/%i)\n>%s< (%i/%i)\n>%s< (%i/%i)\n", ret.BasicIPInfo, baseIPlenUsed, basicIPStringLen, ret.AdditionalIPInfo, nondefaultLenUsed, nondefaultIpStringLen, ret.RouteInfo, routeinfoLenUsed, rounteinfoLen);
#endif

	regfree(&RouteRegex);
	return ret;
}

int main(int argc, char** argv)
{
#ifdef PROFILING
	struct timespec ts_start;
	timespec_get(&ts_start, TIME_UTC);
#endif
	int main_retcode = 0;

	const char* RegexString = "^[ *]+([-_/0-9a-zA-Z]*) +([0-9a-fA-F]+) (\\[([-/_0-9a-zA-Z]+)\\])?.*$";
	int RegexReturnCode;
	RegexReturnCode = regcomp(&branchParsingRegex, RegexString, REG_EXTENDED | REG_NEWLINE);
	if (RegexReturnCode)
	{
		char* regErrorBuf = (char*)malloc(sizeof(char) * 1024);
		if (regErrorBuf == NULL) ABORT_NO_MEMORY;
		int elen = regerror(RegexReturnCode, &branchParsingRegex, regErrorBuf, 1024);
		printf("Could not compile regular expression '%s'. [%i(%s) {len:%i}]\n", RegexString, RegexReturnCode, regErrorBuf, elen);
		free(regErrorBuf);
		exit(1);
	};

	//stuff to obtain the time/date/calendarweek for prompt
	time_t tm;
	tm = time(NULL);
	const struct tm* localtm = localtime(&tm);

	bool IsPrompt = 0, IsSet = 0, IsShow = 0, IsList = 0, IsLowPrompt = 0;

	bool IsThoroughSearch = 0;
	int PromptRetCode = 0;
	int Arg_TotalPromptWidth = 0;
	const char* Arg_NewRemote = NULL;

	const char* Arg_CmdTime = NULL;

	char* User = ExecuteProcess_alloc("whoami");
	TerminateStrOn(User, DEFAULT_TERMINATORS);
	int User_len = strlen_visible(User);

	char* Host = ExecuteProcess_alloc("hostname");
	TerminateStrOn(Host, DEFAULT_TERMINATORS);
	int Host_len = strlen_visible(Host);

	char* Arg_TerminalDevice = NULL;
	int Arg_TerminalDevice_len = 0;

	char* Time = NULL;
	char* TimeZone = NULL;
	char* DateInfo = NULL;
	char* CalendarWeek = NULL;
	int Time_len = 0;
	int TimeZone_len = 0;
	int DateInfo_len = 0;
	int CalendarWeek_len = 0;

	char* Arg_LocalIPs = NULL;
	int Arg_LocalIPs_len = 0;

	char* Arg_LocalIPsAdditional = NULL;
	int Arg_LocalIPsAdditional_len = 0;

	char* Arg_LocalIPsRoutes = NULL;
	int Arg_LocalIPsRoutes_len = 0;

	char* Arg_ProxyInfo = NULL;
	int Arg_ProxyInfo_len = 0;

	char* Arg_PowerState = NULL;
	int Arg_PowerState_len = 0;

	char* Arg_BackgroundJobs = NULL;
	int Arg_BackgroundJobs_len = 0;

	char* Arg_SHLVL = NULL;
	int Arg_SHLVL_len = 0;

	char* Arg_SSHInfo = NULL;
	int Arg_SSHInfo_len = 0;

	IP_MODE ipMode = IP_MODE_NONE;

	int getopt_currentChar;//the char for getop switch

	while (1) {
		int option_index = 0;

		const static struct option long_options[] = {
			{"prompt", no_argument, 0, '0' },
			{"show", no_argument, 0, '1' },
			{"set", no_argument, 0, '2' },
			{"list", no_argument, 0, '3' },
			{"lowprompt", no_argument, 0, '4' },
			{"branchlimit", required_argument, 0, 'b' },
			{"thorough", no_argument, 0, 'f'},
			{"quick", no_argument, 0, 'q'},
			{"help", no_argument, 0, 'h' },
			{0, 0, 0, 0 }
		};

		getopt_currentChar = getopt_long(argc, argv, "b:c:d:e:fhi:Ij:l:n:p:qr:s:t:", long_options, &option_index);
		if (getopt_currentChar == -1)
			break;

		switch (getopt_currentChar) {
		case 0:
			{
				printf("long option %s", long_options[option_index].name);
				if (optarg)
					printf(" with arg %s", optarg);
				printf("\n");
				break;
			}
		case '0':
			{
				if (IsSet || IsShow || IsList) {
					printf("prompt is mutex with set|show|list|lowprompt\n");
					break;
				}
				IsPrompt = 1;
				break;
			}
		case '1':
			{
				if (IsSet || IsPrompt || IsList) {
					printf("show is mutex with set|prompt|list|lowprompt\n");
					break;
				}
				IsShow = 1;
				break;
			}
		case '2':
			{
				if (IsPrompt || IsShow || IsList) {
					printf("set is mutex with prompt|show|list|lowprompt\n");
					break;
				}
				IsSet = 1;
				break;
			}
		case '3':
			{
				if (IsPrompt || IsShow || IsSet) {
					printf("list is mutex with prompt|show|set|lowprompt\n");
					break;
				}
				IsList = 1;
				break;
			}
		case '4':
			{
				if (IsPrompt || IsShow || IsSet || IsList) {
					printf("list is mutex with prompt|show|set|list\n");
					break;
				}
				IsLowPrompt = 1;
				break;
			}
		case 'b':
			{
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				CONFIG_GIT_MAXBRANCHES = atoi(optarg);
				break;
			}
		case 'c':
			{
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_TotalPromptWidth = atoi(optarg);
				break;
			}
		case 'd':
			{
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_TerminalDevice = optarg;
				Arg_TerminalDevice_len = strlen_visible(Arg_TerminalDevice);
				break;
			}
		case 'e':
			{
				//since the output will be sent through an escape sequence processing system to do the colours and stuff I must replace the literal % for the battery with a %% (the literal % is marked by \a%)
				char* temp = optarg;
				while (*temp != 0x00) {
					if (*temp == '\a' && *(temp + 1) == '%') {
						*temp = '%';
					}
					temp++;
				}
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_PowerState = optarg;
				Arg_PowerState_len = strlen_visible(Arg_PowerState);
				break;
			}

		case 'f':
			{
				IsThoroughSearch = 1;
				break;
			}
		case 'h':
			{
				printf("TODO: create a help utility\n");
				break;
			}
		case 'i':
			{
				if (ipMode != IP_MODE_NONE) {
					fprintf(stderr, "WARNING: multiple -I/-i found, only the last -i will be used, everything else will be discarded");
				}
				ipMode = IP_MODE_LEGACY;
				if (Arg_LocalIPs != NULL) {
					Arg_LocalIPs_len = 0;
					free(Arg_LocalIPs);
					Arg_LocalIPs = NULL;
				}
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				if (asprintf(&Arg_LocalIPs, "%s", optarg) == -1) ABORT_NO_MEMORY;
				Arg_LocalIPs_len = strlen_visible(Arg_LocalIPs);
				Arg_LocalIPsAdditional = (char*)malloc(sizeof(char) * 1);
				if (Arg_LocalIPsAdditional == NULL) ABORT_NO_MEMORY;
				Arg_LocalIPsAdditional[0] = 0x00;
				Arg_LocalIPsAdditional_len = 0;
				Arg_LocalIPsRoutes = (char*)malloc(sizeof(char) * 1);
				if (Arg_LocalIPsRoutes == NULL) ABORT_NO_MEMORY;
				Arg_LocalIPsRoutes[0] = 0x00;
				Arg_LocalIPsRoutes_len = 0;
				break;
			}
		case 'I':
			{
				if (ipMode == IP_MODE_LEGACY) {
					fprintf(stderr, "ONLY -i OR -I are supported, they are mutually exclusive\nutilizing Legacy mode (-i)\n");
					break;
				}
				ipMode = IP_MODE_STANDALONE;
				break;
			}
		case 'j':
			{
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_BackgroundJobs = optarg;
				Arg_BackgroundJobs_len = strlen_visible(Arg_BackgroundJobs);
				break;
			}
		case 'l':
			{
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_SHLVL = optarg;
				//if the shell level is 1, don't print it, only if shell level >1 show it
				if (Compare(Arg_SHLVL, " [1]")) {
					Arg_SHLVL[0] = 0x00;
				}
				Arg_SHLVL_len = strlen_visible(Arg_SHLVL);
				break;
			}
		case 'n':
			{
				//printf("option n: %s", optarg);fflush(stdout);
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_NewRemote = optarg;
				break;
			}
		case 'p':
			{
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_ProxyInfo = optarg;
				Arg_ProxyInfo_len = strlen_visible(Arg_ProxyInfo);
				break;
			}
		case 'q':
			{
				IsThoroughSearch = false;
				break;
			}
		case 'r':
			{
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				PromptRetCode = atoi(optarg);
				break;
			}
		case 's':
			{
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_SSHInfo = optarg;
				Arg_SSHInfo_len = strlen_visible(Arg_SSHInfo);
				break;
			}
		case 't':
			{
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_CmdTime = optarg;
				break;
			}
		case '?':
			{
				printf("option %c: >%s<\n", getopt_currentChar, optarg);
				break;
			}
		default:
			{
				printf("?? getopt returned character code 0%o (char: %c) with option %s ??\n", getopt_currentChar, getopt_currentChar, optarg);
			}
		}
	}

	if (!(IsPrompt || IsShow || IsSet || IsList || IsLowPrompt)) {
		printf("you must specify EITHER --prompt --set --show --list or --lowprompt\n");
		exit(1);
	}

	if (!IsList && !(optind < argc)) {
		printf("You must supply one non-option parameter (if not in --list mode)");
	}
	const char* path = argv[optind];


	if (IsSet || IsShow || IsPrompt || IsLowPrompt) {
		DoSetup();//this reads the config file -> as of hereI can expect to have current options
	}

	if (ipMode == IP_MODE_STANDALONE) {
		if (Arg_LocalIPs == NULL && Arg_LocalIPs_len == 0) {
			//at this point ArgLocalIPs should ALWAYS be NULL, but for sanity's sake I'll check again anyway
			//only do the own lookup if IP hasn't been passed in in the old format.
			//if this happens the user is just stuck on the old system but it's functional
			IpTransportStruct temp = GetBaseIPString();
			Arg_LocalIPs = temp.BasicIPInfo;
			Arg_LocalIPs_len = strlen_visible(Arg_LocalIPs);
			Arg_LocalIPsAdditional = temp.AdditionalIPInfo;
			Arg_LocalIPsAdditional_len = strlen_visible(Arg_LocalIPsAdditional);
			Arg_LocalIPsRoutes = temp.RouteInfo;
			Arg_LocalIPsRoutes_len = strlen_visible(Arg_LocalIPsRoutes);
		}
	}
	else if (ipMode == IP_MODE_LEGACY && !CONFIG_PROMPT_NETWORK) {
		//if IP is disabled in config but legagy IP has been provided, remove the info
		if (Arg_LocalIPs != NULL) free(Arg_LocalIPs);
		Arg_LocalIPs = (char*)malloc(sizeof(char) * 1);
		if (Arg_LocalIPs == NULL) ABORT_NO_MEMORY;
		Arg_LocalIPs[0] = 0x00;
		Arg_LocalIPs_len = 0;
	}
	if (!CONFIG_PROMPT_POWER && Arg_PowerState != NULL) { Arg_PowerState[0] = 0x00; Arg_PowerState_len = 0; }
	if (!CONFIG_PROMPT_PROXY && Arg_ProxyInfo != NULL) { Arg_ProxyInfo[0] = 0x00; Arg_ProxyInfo_len = 0; }
	if (!CONFIG_PROMPT_SSH && Arg_SSHInfo != NULL) { Arg_SSHInfo[0] = 0x00; Arg_SSHInfo_len = 0; }
	if (!CONFIG_PROMPT_TERMINAL_DEVICE && Arg_TerminalDevice != NULL) { Arg_TerminalDevice[0] = 0x00; Arg_TerminalDevice_len = 0; }
	if (!CONFIG_PROMPT_JOBS && Arg_BackgroundJobs != NULL) { Arg_BackgroundJobs[0] = 0x00; Arg_BackgroundJobs_len = 0; }
	Time = malloc(sizeof(char) * 16);
	if (Time == NULL) ABORT_NO_MEMORY;
	if (CONFIG_PROMPT_TIME) {
		Time_len = strftime(Time, 16, "%T", localtm);
	}
	else {
		Time_len = 0;
		Time[0] = 0x00;
	}

	TimeZone = malloc(sizeof(char) * 17);
	if (TimeZone == NULL) ABORT_NO_MEMORY;
	if (CONFIG_PROMPT_TIMEZONE) {
		TimeZone_len = strftime(TimeZone, 17, " UTC%z (%Z)", localtm);
	}
	else {
		TimeZone_len = 0;
		TimeZone[0] = 0x00;
	}

	DateInfo = malloc(sizeof(char) * 16);
	if (DateInfo == NULL) ABORT_NO_MEMORY;
	if (CONFIG_PROMPT_DATE) {
		DateInfo_len = strftime(DateInfo, 16, " %a %d.%m.%Y", localtm);
	}
	else {
		DateInfo_len = 0;
		DateInfo[0] = 0x00;
	}

	CalendarWeek = malloc(sizeof(char) * 8);
	if (CalendarWeek == NULL) ABORT_NO_MEMORY;
	if (CONFIG_PROMPT_CALENDARWEEK) {
		CalendarWeek_len = strftime(CalendarWeek, 8, " KW%V", localtm);
	}
	else {
		CalendarWeek_len = 0;
		CalendarWeek[0] = 0x00;
	}


#ifdef DEBUG
	printf("Arg_NewRemote: >%s< (n/a)\n", Arg_NewRemote);fflush(stdout);
	printf("User: >%s< (%i)\n", User, User_len);fflush(stdout);
	printf("Host: >%s< (%i)\n", Host, Host_len);fflush(stdout);
	printf("Arg_TerminalDevice: >%s< (%i)\n", Arg_TerminalDevice, Arg_TerminalDevice_len);fflush(stdout);
	printf("Time: >%s< (%i)\n", Time, Time_len);fflush(stdout);
	printf("TimeZone: >%s< (%i)\n", TimeZone, TimeZone_len);fflush(stdout);
	printf("DateInfo: >%s< (%i)\n", DateInfo, DateInfo_len);fflush(stdout);
	printf("CalendarWeek: >%s< (%i)\n", CalendarWeek, CalendarWeek_len);fflush(stdout);
	printf("Arg_LocalIPs: >%s< (%i)\n", Arg_LocalIPs, Arg_LocalIPs_len);fflush(stdout);
	printf("Arg_LocalIPsAdditional: >%s< (%i)\n", Arg_LocalIPsAdditional, Arg_LocalIPsAdditional_len);fflush(stdout);
	printf("Arg_LocalIPsRoutes: >%s< (%i)\n", Arg_LocalIPsRoutes, Arg_LocalIPsRoutes_len);fflush(stdout);
	printf("Arg_ProxyInfo: >%s< (%i)\n", Arg_ProxyInfo, Arg_ProxyInfo_len);fflush(stdout);
	printf("Arg_PowerState: >%s< (%i)\n", Arg_PowerState, Arg_PowerState_len);fflush(stdout);
	printf("Arg_BackgroundJobs: >%s< (%i)\n", Arg_BackgroundJobs, Arg_BackgroundJobs_len);fflush(stdout);
	printf("Arg_SHLVL: >%s< (%i)\n", Arg_SHLVL, Arg_SHLVL_len);fflush(stdout);
	printf("Arg_SSHInfo: >%s< (%i)\n", Arg_SSHInfo, Arg_SSHInfo_len);fflush(stdout);
	for (int i = 0;i < argc;i++) {
		printf("%soption-arg %i:\t>%s<\n", (i >= optind ? "non-" : "\t"), i, argv[i]);
	}
	fflush(stdout);
#endif

	if (CONFIG_GIT_MAXBRANCHES == -2) {
		//if IsPrompt, default 25; if IsSet, default 50; else default -1
		CONFIG_GIT_MAXBRANCHES = (IsPrompt ? CONFIG_PROMPT_GIT_BRANCHLIMIT : (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_BRANCHLIMIT : CONFIG_LSGIT_QUICK_BRANCHLIMIT));
	}

	CONFIG_GIT_WARN_BRANCH_LIMIT = IsPrompt ? CONFIG_PROMPT_GIT_WARN_BRANCHLIMIT : CONFIG_LSGIT_WARN_BRANCHLIMIT;

	CONFIG_GIT_REPOTYPE = IsPrompt ? CONFIG_PROMPT_GIT_REPOTYPE : (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_REPOTYPE : CONFIG_LSGIT_QUICK_REPOTYPE);
	CONFIG_GIT_REPOTYPE_PARENT = IsPrompt ? CONFIG_PROMPT_GIT_REPOTYPE_PARENT : (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_REPOTYPE_PARENT : CONFIG_LSGIT_QUICK_REPOTYPE_PARENT);
	CONFIG_GIT_REPONAME = IsPrompt ? CONFIG_PROMPT_GIT_REPONAME : (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_REPONAME : CONFIG_LSGIT_QUICK_REPONAME);
	CONFIG_GIT_BRANCHNAME = IsPrompt ? CONFIG_PROMPT_GIT_BRANCHNAME : (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_BRANCHNAME : CONFIG_LSGIT_QUICK_BRANCHNAME);
	CONFIG_GIT_BRANCH_OVERVIEW = IsPrompt ? CONFIG_PROMPT_GIT_BRANCHINFO : (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_BRANCHINFO : CONFIG_LSGIT_QUICK_BRANCHINFO);
	CONFIG_GIT_BRANCHSTATUS = IsPrompt ? CONFIG_PROMPT_GIT_BRANCHSTATUS : (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_BRANCHSTATUS : CONFIG_LSGIT_QUICK_BRANCHSTATUS);
	CONFIG_GIT_REMOTE = IsPrompt ? CONFIG_PROMPT_GIT_REMOTE : (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_REMOTE : CONFIG_LSGIT_QUICK_REMOTE);
	CONFIG_GIT_COMMIT_OVERVIEW = IsPrompt ? CONFIG_PROMPT_GIT_COMMITS : (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_COMMITS : CONFIG_LSGIT_QUICK_COMMITS);
	CONFIG_GIT_LOCALCHANGES = IsPrompt ? CONFIG_PROMPT_GIT_GITSTATUS : (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_GITSTATUS : CONFIG_LSGIT_QUICK_GITSTATUS);

	if (IsPrompt) //show origin info for command prompt
	{
		if (CONFIG_PROMPT_OVERALL_ENABLE) {
			//this is intentionally not OR-ed with IsPrompt as IsPrompt is in an exhaustive if/else where if this would evaluate to false I would get an error to the effect of "unknown option PROMPT"


			//taking the list of jobs as input, this counts the number of spaces (and because of the trailing space also the number of entries)
			int numBgJobs = 0;
			if (Arg_BackgroundJobs_len > 0) {
				int i = 0;
				while (Arg_BackgroundJobs[i] != 0x00) {
					if (Arg_BackgroundJobs[i] == ' ') {
						numBgJobs++;
					}
					i++;
				}
			}

			char* numBgJobsStr;
			if (numBgJobs != 0) {
				if (asprintf(&numBgJobsStr, "  %i Jobs", numBgJobs) == -1) ABORT_NO_MEMORY;
			}
			else {
				numBgJobsStr = (char*)malloc(sizeof(char));
				if (numBgJobsStr == NULL) ABORT_NO_MEMORY;
				numBgJobsStr[0] = 0x00;
			}


			RepoInfo* ri = AllocRepoInfo("", path);
			//only test git if git is enabled at all
			if (CONFIG_PROMPT_GIT && !TestPathForRepoAndParseIfExists(ri, -1, true, true)) {
				//if TestPathForRepoAndParseIfExists fails it'll do it's own cleanup (= deallocation etc)
				fprintf(stderr, "error at main: TestPathForRepoAndParseIfExists returned null\n");
				return 1;
			}
			AllocUnsetStringsToEmpty(ri);

			char* gitSegment1_BaseMarkerStart = NULL;
			char* gitSegment2_parentRepoLoc = gitSegment1_BaseMarkerStart;//just an empty default
			char* gitSegment3_BaseMarkerEnd = gitSegment1_BaseMarkerStart;
			char* gitSegment4_remoteinfo = gitSegment1_BaseMarkerStart;
			char* gitSegment5_commitStatus = gitSegment1_BaseMarkerStart;
			char* gitSegment6_gitStatus = gitSegment1_BaseMarkerStart;
			int gitSegment1_BaseMarkerStart_len = 0;
			int gitSegment2_parentRepoLoc_len = 0;
			int gitSegment3_BaseMarkerEnd_len = 0;
			int gitSegment4_remoteinfo_len = 0;
			int gitSegment5_commitStatus_len = 0;
			int gitSegment6_gitStatus_len = 0;

			if (CONFIG_PROMPT_GIT && ri->isGit) {
				if (CONFIG_GIT_REPOTYPE) {
					if (asprintf(&gitSegment1_BaseMarkerStart, " [" COLOUR_GIT_BARE "%s" COLOUR_GIT_INDICATOR "GIT%s", ri->isBare ? "BARE " : "", ri->isSubModule ? "-SM" : "") == -1) ABORT_NO_MEMORY;//[%F{006}BARE %F{002}GIT-SM
					if (ri->isSubModule && CONFIG_GIT_REPOTYPE_PARENT) {
						if (asprintf(&gitSegment2_parentRepoLoc, COLOUR_CLEAR "@" COLOUR_GIT_PARENT "%s" COLOUR_CLEAR, ri->parentRepo) == -1) ABORT_NO_MEMORY;
					}
				}
				if (gitSegment1_BaseMarkerStart == NULL) {
					gitSegment1_BaseMarkerStart = malloc(sizeof(char) * 1);
					if (gitSegment1_BaseMarkerStart == NULL) ABORT_NO_MEMORY;
					gitSegment1_BaseMarkerStart[0] = 0x00;
				}
				if (gitSegment2_parentRepoLoc == NULL) {
					gitSegment2_parentRepoLoc = malloc(sizeof(char) * 1);
					if (gitSegment2_parentRepoLoc == NULL) ABORT_NO_MEMORY;
					gitSegment2_parentRepoLoc[0] = 0x00;
				}

				char* gitBranchInfo = NULL;
				if (!ri->isBare && CONFIG_GIT_BRANCHSTATUS) {
					gitBranchInfo = ConstructGitBranchInfoString(ri);
				}
				else {
					gitBranchInfo = malloc(sizeof(char));
					if (gitBranchInfo == NULL) ABORT_NO_MEMORY;
					gitBranchInfo[0] = 0x00;
				}
				char* temp1_reponame = NULL;
				char* temp2_branchname = NULL;
				char* temp3_branchoverview = NULL;
				if (asprintf(&temp1_reponame, " "COLOUR_GIT_NAME"%s", ri->RepositoryName) == -1) ABORT_NO_MEMORY;
				if (asprintf(&temp2_branchname, COLOUR_CLEAR " on "COLOUR_GIT_BRANCH "%s", ri->branch) == -1) ABORT_NO_MEMORY;
				if (asprintf(&temp3_branchoverview, "/%i+%i", ri->CountActiveBranches, ri->CountFullyMergedBranches) == -1) ABORT_NO_MEMORY;
				if (asprintf(&gitSegment3_BaseMarkerEnd, COLOUR_CLEAR "%s" "%s%s" COLOUR_GREYOUT "%s%s%s" COLOUR_CLEAR,
					CONFIG_GIT_REPOTYPE ? "]" : "",
					CONFIG_GIT_REPONAME ? temp1_reponame : "",
					CONFIG_GIT_BRANCHNAME ? temp2_branchname : "",
					CONFIG_GIT_BRANCH_OVERVIEW && CONFIG_GIT_BRANCHNAME ? temp3_branchoverview : "",
					(CONFIG_GIT_BRANCHSTATUS && (CONFIG_GIT_BRANCHNAME || CONFIG_GIT_REPONAME)) ? ":" : "",
					gitBranchInfo) == -1) ABORT_NO_MEMORY;

				if (gitBranchInfo != NULL)free(gitBranchInfo);
				if (CONFIG_GIT_REMOTE) {
					if (asprintf(&gitSegment4_remoteinfo, " from " COLOUR_GIT_ORIGIN "%s" COLOUR_CLEAR, ri->RepositoryDisplayedOrigin) == -1) ABORT_NO_MEMORY;
				}
				if (gitSegment4_remoteinfo == NULL) {
					gitSegment4_remoteinfo = malloc(sizeof(char) * 1);
					if (gitSegment4_remoteinfo == NULL) ABORT_NO_MEMORY;
					gitSegment4_remoteinfo[0] = 0x00;
				}

				if (!ri->isBare) {
					if (CONFIG_GIT_COMMIT_OVERVIEW) {
						gitSegment5_commitStatus = ConstructCommitStatusString(ri);
					}
					if (CONFIG_GIT_LOCALCHANGES) {
						gitSegment6_gitStatus = ConstructGitStatusString(ri);
					}
				}

			}
			if (gitSegment1_BaseMarkerStart == NULL) {
				gitSegment1_BaseMarkerStart = malloc(sizeof(char) * 1);
				if (gitSegment1_BaseMarkerStart == NULL)ABORT_NO_MEMORY;
				gitSegment1_BaseMarkerStart[0] = 0x00;
			}
			if (gitSegment2_parentRepoLoc == NULL) {
				gitSegment2_parentRepoLoc = malloc(sizeof(char) * 1);
				if (gitSegment2_parentRepoLoc == NULL)ABORT_NO_MEMORY;
				gitSegment2_parentRepoLoc[0] = 0x00;
			}
			if (gitSegment3_BaseMarkerEnd == NULL) {
				gitSegment3_BaseMarkerEnd = malloc(sizeof(char) * 1);
				if (gitSegment3_BaseMarkerEnd == NULL)ABORT_NO_MEMORY;
				gitSegment3_BaseMarkerEnd[0] = 0x00;
			}
			if (gitSegment4_remoteinfo == NULL) {
				gitSegment4_remoteinfo = malloc(sizeof(char) * 1);
				if (gitSegment4_remoteinfo == NULL)ABORT_NO_MEMORY;
				gitSegment4_remoteinfo[0] = 0x00;
			}
			if (gitSegment5_commitStatus == NULL) {
				gitSegment5_commitStatus = malloc(sizeof(char) * 1);
				if (gitSegment5_commitStatus == NULL)ABORT_NO_MEMORY;
				gitSegment5_commitStatus[0] = 0x00;
			}
			if (gitSegment6_gitStatus == NULL) {
				gitSegment6_gitStatus = malloc(sizeof(char) * 1);
				if (gitSegment6_gitStatus == NULL)ABORT_NO_MEMORY;
				gitSegment6_gitStatus[0] = 0x00;
			}

			gitSegment1_BaseMarkerStart_len = strlen_visible(gitSegment1_BaseMarkerStart);
			gitSegment2_parentRepoLoc_len = strlen_visible(gitSegment2_parentRepoLoc);
			gitSegment3_BaseMarkerEnd_len = strlen_visible(gitSegment3_BaseMarkerEnd);
			gitSegment4_remoteinfo_len = strlen_visible(gitSegment4_remoteinfo);
			gitSegment5_commitStatus_len = strlen_visible(gitSegment5_commitStatus);
			gitSegment6_gitStatus_len = strlen_visible(gitSegment6_gitStatus);


			int RemainingPromptWidth = Arg_TotalPromptWidth - (
				(CONFIG_PROMPT_USER ? User_len : 0) + ((CONFIG_PROMPT_USER && CONFIG_PROMPT_HOST) ? 1 : 0) + (CONFIG_PROMPT_HOST ? Host_len : 0) +
				Arg_SHLVL_len +
				gitSegment1_BaseMarkerStart_len +
				gitSegment3_BaseMarkerEnd_len +
				gitSegment5_commitStatus_len +
				gitSegment6_gitStatus_len +
				Time_len +
				Arg_ProxyInfo_len +
				strlen_visible(numBgJobsStr) +
				Arg_PowerState_len + 2);

#define AdditionalElementCount 11
			uint32_t AdditionalElementAvailabilityPackedBool = determinePossibleCombinations(&RemainingPromptWidth, AdditionalElementCount,
				gitSegment4_remoteinfo_len,
				Arg_LocalIPs_len,
				CalendarWeek_len,
				TimeZone_len,
				DateInfo_len,
				Arg_TerminalDevice_len,
				gitSegment2_parentRepoLoc_len,
				Arg_BackgroundJobs_len,
				Arg_LocalIPsAdditional_len,
				Arg_LocalIPsRoutes_len,
				Arg_SSHInfo_len);

#define AdditionalElementPriorityGitRemoteInfo 0
#define AdditionalElementPriorityLocalIP 1
#define AdditionalElementPriorityCalendarWeek 2
#define AdditionalElementPriorityTimeZone 3
#define AdditionalElementPriorityDate 4
#define AdditionalElementPriorityTerminalDevice 5
#define AdditionalElementPriorityParentRepoLocation 6
#define AdditionalElementPriorityBackgroundJobDetail 7
#define AdditinoalElementPriorityNonDefaultNetworks 8
#define AdditionalElementPriorityRoutingInfo 9
#define AdditionalElementPrioritySSHInfo 10

			//if the seventh-prioritized element (ssh connection info) has space, print it "<SSH: 123.123.13.123:54321 -> 321.312.321.321:22> "
			if (AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPrioritySSHInfo)) {
				printf("%s", Arg_SSHInfo);
			}

			//print username and machine "user@hostname"
			if (CONFIG_PROMPT_USER) {
				printf(COLOUR_USER "%s", User);
				if (CONFIG_PROMPT_HOST) {
					printf(COLOUR_USER_AT_HOST "@");
				}
			}
			if (CONFIG_PROMPT_HOST) {
				printf(COLOUR_HOST "%s", Host);
			}
			printf(COLOUR_CLEAR);

			//if the fourth-prioritized element (the line/terminal device has space, append it to the user@machine) ":/dev/pts/0"
			if (AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityTerminalDevice)) {
				printf(COLOUR_TERMINAL_DEVICE "%s", Arg_TerminalDevice);
			}

			//append the SHLVL (how many shells are nested, ie how many Ctrl+D are needed to properly exit), only shown if >=2 " [2]"
			//also print the first bit of the git indication (is submodule or not) " [GIT-SM"
			printf(COLOUR_SHLVL "%s" COLOUR_CLEAR "%s", Arg_SHLVL, gitSegment1_BaseMarkerStart);

			//if the fifth-prioritized element (the the location of the parent repo, if it exists, ie if this is submodule) "@/some/parentrepo"
			if ((AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityParentRepoLocation))) {
				printf("%s", gitSegment2_parentRepoLoc);
			}

			//print the repo name and branch including the closing bracket arount the git indication "] ShellTools on master"
			printf("%s", gitSegment3_BaseMarkerEnd);

			//if the first-prioritized element (the git remote origin information) has space, print it " from ssh:git@someserver.de:someport"
			if ((AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityGitRemoteInfo))) {
				printf("%s", gitSegment4_remoteinfo);
			}

			//print the last bit of the git indicator (commits to pull, push, stashes, merge-conflict-files, staged changes, non-staged changes, untracked files) " {77⇣ 0⇡} <5M 10+ 3* 4?>"
			printf("%s", gitSegment5_commitStatus);
			printf("%s", gitSegment6_gitStatus);

			//fill the empty space between left and right side
			for (int i = 0;i < RemainingPromptWidth;i++) {
				switch (CONFIG_PROMPT_FILLER_CHAR) {
				case '\\': {
						putc(CONFIG_PROMPT_FILLER_CHAR, stdout);
						putc(CONFIG_PROMPT_FILLER_CHAR, stdout);
						//intentionally missing break; as \ needs more escaping to work
					}
				case '%':
					{
						//since % is an escape char for the prompt string, escape it as %%
						putc(CONFIG_PROMPT_FILLER_CHAR, stdout);
						break;
					}
				case '$':
					{
						/*this is MADNESS
						ZSH needs at least FOUR \ to reliably escape a sequence of two or more $ to prevent them from expanding to the PID, when using print -p "\\\\$\\\\$\\\\$"
						to make things WORSE, when trying this in the shell directly only every other $ needs FOUR \ to be properly escaped, the others are fine with three, but if I go down to three for every $ it will expand to the pid.
						I need to use over EIGHT \ to make a single \ appear when trying this directly in the shell
						And here is where it goes completly nuts:
						when using this from this here progam if I put eight \ in the printf that will be escaped down to four in the actual string since C escapes them as well. IF I use eight \ in this file, therefore four \ in the text a literal \ will appear in the fully escaped output.
						With only \\ or no escapes at all it will be expanded to PID, with \\\\ (or \\\\\\), therefore only two (or three) in the string it works as expected, with eight \ in here, therefore four in the string, a literal \ will appear in the output. The output will remain unchanged up to \\\\\\\\\\\\\\ in here (14 \ therefore 7 in the string) as soon as I go up to \\\\\\\\\\\\\\\\ (16 in here, 8 in the string) a second literal \ appears in the output. I have given up trying to understand this I will just go with four in here and call it a day.*/
						printf("\\\\");
						break;
					}
				default:
					{
						;
					}
				}
				putc(CONFIG_PROMPT_FILLER_CHAR, stdout);
			}

			//print the time in HH:mm:ss "21:24:31"
			printf("%s", Time);

			//if the third-prioritized element (timezone info) has space, print it " UTC+0200 (CEST)"
			if ((AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityTimeZone))) {
				printf("%s", TimeZone);
			}

			if ((AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityDate))) {
				printf("%s", DateInfo);
			}

			if ((AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityCalendarWeek))) {
				printf("%s", CalendarWeek);
			}

			//if a proxy is configured, show it (A=Apt, H=http(s), F=FTP, N=NoProxy) " [AHNF]"
			printf("%s", Arg_ProxyInfo);

			//if the second prioritized element (local IP addresses) has space, print it " eth0:192.168.0.2 wifi0:192.168.0.3"
			if ((AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityLocalIP))) {
				printf("%s", Arg_LocalIPs);
			}

			//Arg_LocalIPsRoutes and Arg_LocalIPsAdditional can AND WILL be NULL if the old IP-system is used (for example on WSL)
			if ((AdditionalElementAvailabilityPackedBool & (1 << AdditinoalElementPriorityNonDefaultNetworks))) {
				printf("%s", Arg_LocalIPsAdditional);
			}

			if ((AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityRoutingInfo))) {
				printf("%s", Arg_LocalIPsRoutes);
			}

			printf("%s", numBgJobsStr);
			//if the sixth-prioritized element (background tasks) has space, print it " {1S-  2S+}"
			if ((AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityBackgroundJobDetail))) {
				printf("%s", Arg_BackgroundJobs);
			}

			//print the battery state (the first unicode char can be any of ▲,≻,▽ or ◊[for not charging,but not discharging]), while the second unicode char indicates the presence of AC power "≻ 100% ↯"
			printf("%s", Arg_PowerState);

			//the last two chars on screen were intentionally empty, I am now printing  ' !' there if ANY additional element had to be omitted
			printf("%s", ~AdditionalElementAvailabilityPackedBool & ~(~0 << AdditionalElementCount) ? " !" : "");


			if (ri->isGit) {
				free(gitSegment1_BaseMarkerStart);
				if (ri->isSubModule) {
					free(gitSegment2_parentRepoLoc);
				}
				free(gitSegment3_BaseMarkerEnd);
				free(gitSegment4_remoteinfo);
				free(gitSegment5_commitStatus);
				free(gitSegment6_gitStatus);
			}
			DeallocRepoInfoStrings(ri);
			free(ri);
			if (Arg_LocalIPs != NULL) {
				free(Arg_LocalIPs);
				Arg_LocalIPs = NULL;
			}
			if (Arg_LocalIPsAdditional != NULL) {
				free(Arg_LocalIPsAdditional);
				Arg_LocalIPsAdditional = NULL;
			}
			if (Arg_LocalIPsRoutes != NULL) {
				free(Arg_LocalIPsRoutes);
				Arg_LocalIPsRoutes = NULL;
			}
		}
	}
	else if (IsSet || IsShow)
	{
		int RequestedNewOriginID = -1;
		if (IsSet) {//a change was requested
			for (int i = 0;i < numLOCS;i++) {
				if (Compare(Arg_NewRemote, NAMES[i])) {//found requested new origin
					RequestedNewOriginID = i;
					break;
				}
			}
			if (RequestedNewOriginID == -1) {
				printf("new origin specification " COLOUR_GIT_ORIGIN "'%s'" COLOUR_CLEAR " is unknown. available origin specifications are:\n", Arg_NewRemote);
				ListAvailableRemotes();
				return -1;
			}
		}
		printf("Performing %s search for repos on %s", IsThoroughSearch ? "thorough" : "quick", path);
		if (RequestedNewOriginID != -1) {
			printf(" and setting my repos to %s", NAMES[RequestedNewOriginID]);
		}
		printf("\n\n");

		RepoInfo* treeroot = CreateDirStruct("", path, RequestedNewOriginID, IsThoroughSearch);
		pruneTreeForGit(treeroot);
		if (RequestedNewOriginID != -1) {
			printf("\n");
		}
		printTree(treeroot, IsThoroughSearch);
		printf("\n");
	}
	else if (IsList) {
		ListAvailableRemotes();
		return 0;
	}
	else if (IsLowPrompt) {
		printf("└%%F{cyan}%%B ");
		if (CONFIG_LOWPROMPT_PATH_LIMIT) {
			int chars = 0;
			switch (CONFIG_LOWPROMPT_PATH_MAXLEN) {
			case -1:
				{
					chars = Arg_TotalPromptWidth / 2;
					//allow a half of the screen width to be current working directory
					break;
				}
			case -2:
				{
					chars = Arg_TotalPromptWidth / 3;
					//allow a third of the screen width to be current working directory
					break;
				}
			case -3:
				{
					chars = Arg_TotalPromptWidth / 4;
					//allow a quarter of the screen width to be current working directory
					break;
				}
			case -4:
				{
					chars = Arg_TotalPromptWidth / 6;
					//allow a sixth of the screen width to be current working directory
					break;
				}
			default: {
					chars = CONFIG_LOWPROMPT_PATH_MAXLEN;
				}
			}
			int segments = chars / 16; //impose a limit on how many segments make sense. if the terminal is 256 wide, chars would be 64 (with my default 1/4 setup) and I would allow 4 segements
			char* temp;
			AbbreviatePathAuto(&temp, path, chars, segments);
			printf("%s", temp);
			free(temp);
			temp = NULL;
		}
		else {
			printf("%s", path);
		}
		//fprintf(stderr, "(%i %i)%s | %s\n", chars, segments, temp, path);
		printf("%s ", (PromptRetCode == 0 ? "%F{green}" : "%F{red}"));
		if (CONFIG_LOWPROMPT_RETCODE || CONFIG_LOWPROMPT_TIMER) {
			printf("[");
			if (CONFIG_LOWPROMPT_TIMER) {
				printf("%s", Arg_CmdTime);
				if (CONFIG_LOWPROMPT_RETCODE) {
					printf(":");
				}
			}
			if (CONFIG_LOWPROMPT_RETCODE) {
				printf("%i", PromptRetCode);
				if (CONFIG_LOWPROMPT_RETCODE_DECODE) {
					switch (PromptRetCode) {
					case 0:
						printf(" (OK)");
						break;
					case 2:
						printf(" (arg-error/incorrect usage)");
						break;
					case 126:
						printf(" (no permission/not executable)");
						break;
					case 127:
						printf(" (cmd not found)");
						break;
					case 128 + SIGHUP:
						printf(" (SIGHUP)");
						break;
					case 128 + SIGINT:
						printf(" (SIGINT)");
						break;
					case 128 + SIGQUIT:
						printf(" (SIGQUIT)");
						break;
					case 128 + SIGILL:
						printf(" (SIGILL)");
						break;
					case 128 + SIGTRAP:
						printf(" (SIGTRAP)");
						break;
					case 128 + SIGABRT:
						printf(" (SIGABRT)");
						break;
					case 128 + SIGFPE:
						printf(" (SIGFPE)");
						break;
					case 128 + SIGKILL:
						printf(" (SIGKILL)");
						break;
					case 128 + SIGSEGV:
						printf(" (SIGSEGV)");
						break;
					case 128 + SIGPIPE:
						printf(" (SIGPIPE)");
						break;
					case 128 + SIGALRM:
						printf(" (SIGALRM)");
						break;
					case 128 + SIGTERM:
						printf(" (SIGTERM)");
						break;
					case 128 + SIGSTKFLT:
						printf(" (SIGSTKFLT)");
						break;
					case 128 + SIGPWR:
						printf(" (SIGPWR)");
						break;
					case 128 + SIGBUS:
						printf(" (SIGBUS)");
						break;
					case 128 + SIGSYS:
						printf(" (SIGSYS)");
						break;
					case 128 + SIGURG:
						printf(" (SIGURG)");
						break;
					case 128 + SIGSTOP:
						printf(" (SIGSTOP)");
						break;
					case 128 + SIGTSTP:
						printf(" (SIGTSTP)");
						break;
					case 128 + SIGCONT:
						printf(" (SIGCONT)");
						break;
					case 128 + SIGCHLD:
						printf(" (SIGCHLD)");
						break;
					case 128 + SIGTTIN:
						printf(" (SIGTTIN)");
						break;
					case 128 + SIGTTOU:
						printf(" (SIGTTOU)");
						break;
					case 128 + SIGPOLL:
						printf(" (SIGPOLL)");
						break;
					case 128 + SIGXFSZ:
						printf(" (SIGXFSZ)");
						break;
					case 128 + SIGXCPU:
						printf(" (SIGXCPU)");
						break;
					case 128 + SIGVTALRM:
						printf(" (SIGVTALRM)");
						break;
					case 128 + SIGPROF:
						printf(" (SIGPROF)");
						break;
					case 128 + SIGUSR1:
						printf(" (SIGUSR1)");
						break;
					case 128 + SIGUSR2:
						printf(" (SIGUSR2)");
						break;
					case 128 + SIGWINCH:
						printf(" (SIGWINCH)");
						break;
					default:
						break;
					}
				}
			}
			printf("]");
		}
		printf("➜%%f%%b  ");
	}
	else {
		printf("unknown command %s\n", argv[1]);
		return -1;
	}

	if (IsSet || IsShow || IsPrompt || IsLowPrompt) {
		Cleanup();
	}
	regfree(&branchParsingRegex);

#ifdef PROFILING
	struct timespec ts_end;
	timespec_get(&ts_end, TIME_UTC);
	uint64_t tstart = ts_start.tv_sec * 1000ULL + (ts_start.tv_nsec / 1000000ULL);
	uint64_t tstop = ts_end.tv_sec * 1000ULL + (ts_end.tv_nsec / 1000000ULL);
	printf("\n%lu", tstop - tstart);
#endif

	return main_retcode;
}
