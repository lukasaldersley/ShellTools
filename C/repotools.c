#/*
TargetDir="$ST_CFG"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename $0 .c).elf"
printf "compiling $0 into $TargetDir/$TargetName"
gcc -O3 -std=c2x -Wall "$(realpath "$(dirname "$0")")"/commons.c "$0" -o "$TargetDir/$TargetName" $1
#the fact I DIDN'T add quotations to the $1 above means I WANT word splitting to happen.
#I WANT to be able to do things like ./repotools.c -DPROFILING to add the compiler flag profiling but ALSO stuff like ./repotools "-DDEBUG -DPROFILING" to add both profiling and debug
printf " -> \e[32mDONE\e[0m($?)\n"
exit
*/

#include "commons.h"
#include <regex.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <dirent.h>
#include <time.h>
#include <getopt.h>
#include <signal.h>

#define COLOUR_GIT_BARE "\e[38;5;006m"
#define COLOUR_GIT_INDICATOR "\e[38;5;002m"
#define COLOUR_GIT_PARENT "\e[38;5;004m"
#define COLOUR_GIT_NAME "\e[38;5;009m"
#define COLOUR_GIT_BRANCH "\e[38;5;001m"
#define COLOUR_GIT_ORIGIN "\e[38;5;005m"
#define COLOUR_GIT_COMMITS "\e[38;5;208m"
#define COLOUR_GIT_MERGES "\e[38;5;009m"
#define COLOUR_GIT_STAGED "\e[38;5;010m"
#define COLOUR_GIT_MODIFIED "\e[38;5;226m"
#define COLOUR_GIT_BRANCH_REMOTEONLY "\e[0m"
#define COLOUR_GIT_BRANCH_LOCALONLY "\e[38;5;220m"
#define COLOUR_GIT_BRANCH_UNEQUAL "\e[38;5;009m"
#define COLOUR_TERMINAL_DEVICE "\e[38;5;242m"
#define COLOUR_SHLVL "\e[0m"
#define COLOUR_USER "\e[38;5;010m"
#define COLOUR_USER_AT_HOST "\e[38;5;007m"
#define COLOUR_HOST "\e[38;5;033m"
const char* terminators = "\r\n\a";

#define maxGroups 10
regmatch_t CapturedResults[maxGroups];
regex_t reg;

#define MaxLocations 10
char* NAMES[MaxLocations];
char* LOCS[MaxLocations];
char* GitHubs[MaxLocations];
uint8_t numGitHubs = 0;
int LIMIT_BRANCHES = -2;
uint8_t numLOCS = 0;
#define DEFAULT_CONFIG_FILE "\
# Lines starting with # are Comments, seperator between Entries is TAB (Spaces DO NOT WORK [by design])\n\
ORIGIN:	NONE	/dev/null\n\
ORIGIN:	LOOPBACK	ssh://git@127.0.0.1/data/repos\n\
HOST:	github.com\n\
HOST:	gitlab.com\n"

typedef struct {
	char* BranchName;
	bool IsMergedLocal;
	char* CommitHashLocal;
	bool IsMergedRemote;
	char* CommitHashRemote;
} BranchInfo;

typedef struct BranchListSorted_t BranchListSorted;
struct BranchListSorted_t {
	BranchInfo branchinfo;
	BranchListSorted* next;
	BranchListSorted* prev;
};

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
	char* RepositoryUnprocessedOrigin_PREVIOUS;
	char* branch;
	char* parentRepo;
	int SubDirectoryCount;
	int RepositoryOriginID;
	int RepositoryOriginID_PREVIOUS;
	bool isGit;
	bool isSubModule;
	bool isBare;
	bool DirtyWorktree;
	bool HasRemote;
	int RemoteCommits;
	int LocalCommits;
	int UntrackedFiles;
	int ModifiedFiles;
	int StagedChanges;
	int ActiveMergeFiles;
	int stashes;

	int CountRemoteOnlyBranches;
	int CountLocalOnlyBranches;
	int CountUnequalBranches;
	int CountActiveBranches;
	int CountFullyMergedBranches;
	bool CheckedOutBranchIsNotInRemote;
};

RepoInfo* AllocRepoInfo(const char* directoryPath, const char* directoryName) {
	RepoInfo* ri = (RepoInfo*)malloc(sizeof(RepoInfo));
	if (ri == NULL)abortNomem();
	ri->ParentDirectory = NULL;
	ri->SubDirectories = NULL;
	ri->DirectoryName = NULL;
	ri->DirectoryPath = NULL;
	ri->RepositoryName = NULL;
	ri->RepositoryDisplayedOrigin = NULL;
	ri->RepositoryUnprocessedOrigin = NULL;
	ri->RepositoryUnprocessedOrigin_PREVIOUS = NULL;
	ri->branch = NULL;
	ri->parentRepo = NULL;
	ri->SubDirectoryCount = 0;
	ri->RepositoryOriginID = -1;
	ri->RepositoryOriginID_PREVIOUS = -1;
	ri->isGit = false;
	ri->isSubModule = false;
	ri->isBare = false;
	ri->DirtyWorktree = false;
	ri->HasRemote = false;
	ri->RemoteCommits = 0;
	ri->LocalCommits = 0;
	ri->UntrackedFiles = 0;
	ri->ModifiedFiles = 0;
	ri->StagedChanges = 0;
	ri->ActiveMergeFiles = 0;
	ri->stashes = 0;

	ri->CountRemoteOnlyBranches = 0;
	ri->CountLocalOnlyBranches = 0;
	ri->CountUnequalBranches = 0;
	ri->CountActiveBranches = 0;
	ri->CountFullyMergedBranches = 0;
	ri->CheckedOutBranchIsNotInRemote = false;

	if (asprintf(&(ri->DirectoryName), "%s", directoryName) == -1)abortNomem();
	if (asprintf(&(ri->DirectoryPath), "%s%s%s", directoryPath, ((strlen(directoryPath) > 0) ? "/" : ""), directoryName) == -1)abortNomem();
	return ri;
}

void DeallocRepoInfoStrings(RepoInfo* ri) {
	if (ri->DirectoryName != NULL) { free(ri->DirectoryName); }
	if (ri->DirectoryPath != NULL) { free(ri->DirectoryPath); }
	if (ri->RepositoryName != NULL) { free(ri->RepositoryName); }
	if (ri->RepositoryDisplayedOrigin != NULL) { free(ri->RepositoryDisplayedOrigin); }
	if (ri->RepositoryUnprocessedOrigin != NULL) { free(ri->RepositoryUnprocessedOrigin); }
	if (ri->RepositoryUnprocessedOrigin_PREVIOUS != NULL) { free(ri->RepositoryUnprocessedOrigin_PREVIOUS); }
	if (ri->branch != NULL) { free(ri->branch); }
	if (ri->parentRepo != NULL) { free(ri->parentRepo); }
}

void AllocUnsetStringsToEmpty(RepoInfo* ri) {
	if (ri->DirectoryName == NULL) { ri->DirectoryName = (char*)malloc(sizeof(char));ri->DirectoryName[0] = 0x00; }
	if (ri->DirectoryPath == NULL) { ri->DirectoryPath = (char*)malloc(sizeof(char));ri->DirectoryPath[0] = 0x00; }
	if (ri->RepositoryName == NULL) { ri->RepositoryName = (char*)malloc(sizeof(char));ri->RepositoryName[0] = 0x00; }
	if (ri->RepositoryDisplayedOrigin == NULL) { ri->RepositoryDisplayedOrigin = (char*)malloc(sizeof(char));ri->RepositoryDisplayedOrigin[0] = 0x00; }
	if (ri->RepositoryUnprocessedOrigin == NULL) { ri->RepositoryUnprocessedOrigin = (char*)malloc(sizeof(char));ri->RepositoryUnprocessedOrigin[0] = 0x00; }
	if (ri->RepositoryUnprocessedOrigin_PREVIOUS == NULL) { ri->RepositoryUnprocessedOrigin_PREVIOUS = (char*)malloc(sizeof(char));ri->RepositoryUnprocessedOrigin_PREVIOUS[0] = 0x00; }
	if (ri->branch == NULL) { ri->branch = (char*)malloc(sizeof(char));ri->branch[0] = 0x00; }
	if (ri->parentRepo == NULL) { ri->parentRepo = (char*)malloc(sizeof(char));ri->parentRepo[0] = 0x00; }
}

BranchListSorted* InitBranchListSortedElement() {
	BranchListSorted* a = (BranchListSorted*)malloc(sizeof(BranchListSorted));
	if (a == NULL)abortNomem();
	BranchInfo* e = &(a->branchinfo);
	e->BranchName = NULL;
	e->CommitHashLocal = NULL;
	e->IsMergedLocal = false;
	e->CommitHashRemote = NULL;
	e->IsMergedRemote = false;
	return a;
}

BranchListSorted* InsertIntoBranchListSorted(BranchListSorted* head, char* branchname, char* hash, bool remote) {
	if (head == NULL) {
		//Create Initial Element (List didn't exist previously)
		BranchListSorted* n = InitBranchListSortedElement();
		n->next = NULL;
		n->prev = NULL;
		if (asprintf(&(n->branchinfo.BranchName), "%s", branchname) == -1)abortNomem();
		if (remote) {
			if (asprintf(&(n->branchinfo.CommitHashRemote), "%s", hash) == -1)abortNomem();
		}
		else {
			if (asprintf(&(n->branchinfo.CommitHashLocal), "%s", hash) == -1)abortNomem();
		}
		return n;
	}
	else if (Compare(head->branchinfo.BranchName, branchname)) {
		//Branch Name matches, this means I know of the local copy and am adding the remote one or vice versa
		if (remote) {
			if (asprintf(&(head->branchinfo.CommitHashRemote), "%s", hash) == -1)abortNomem();
		}
		else {
			if (asprintf(&(head->branchinfo.CommitHashLocal), "%s", hash) == -1)abortNomem();
		}
		return head;
	}
	else if (CompareStrings(head->branchinfo.BranchName, branchname) == ALPHA_AFTER) {
		//The new Element is Alphabetically befory myself -> insert new element before myself
		BranchListSorted* n = InitBranchListSortedElement();
		n->next = head;
		n->prev = head->prev;
		if (head->prev != NULL) head->prev->next = n;
		head->prev = n;
		if (asprintf(&(n->branchinfo.BranchName), "%s", branchname) == -1)abortNomem();
		if (remote) {
			if (asprintf(&(n->branchinfo.CommitHashRemote), "%s", hash) == -1)abortNomem();
		}
		else {
			if (asprintf(&(n->branchinfo.CommitHashLocal), "%s", hash) == -1)abortNomem();
		}
		return n;
	}
	else if (head->next == NULL) {
		//The New Element is NOT Equal to myself and is NOT alphabetically before myself (as per the earlier checks)
		//on top of that, I AM the LAST element, so I can simply create a new last element
		BranchListSorted* n = InitBranchListSortedElement();
		n->next = head->next;
		n->prev = head;
		if (head->next != NULL) head->next->prev = n;
		head->next = n;
		if (asprintf(&(n->branchinfo.BranchName), "%s", branchname) == -1)abortNomem();
		if (remote) {
			if (asprintf(&(n->branchinfo.CommitHashRemote), "%s", hash) == -1)abortNomem();
		}
		else {
			if (asprintf(&(n->branchinfo.CommitHashLocal), "%s", hash) == -1)abortNomem();
		}
		return head;
	}
	else {
		//The New Element is NOT Equal to myslef and is NOT alphabetically before myself (as per the earlier checks)
		//This time there IS another element after myself -> defer to it.
		head->next = InsertIntoBranchListSorted(head->next, branchname, hash, remote);
		return head;
	}
}

bool IsMergeCommit(const char* repoPath, const char* commitHash) {
	const int size = 32;
	char* result = (char*)malloc(sizeof(char) * size);
	char* cmd;
	bool RES = false;
	//this looks up the commit hashes for all parents of a given commit
	//usually a commit has EXACTLY ONE parent, the only exceptions are the initial commit (no parents) or a merge commit (two parents)
	//therefore if a commit has 2 parents it's a merge commit
	if (asprintf(&cmd, "git -C \"%s\" cat-file -p %s |grep parent|wc -l", repoPath, commitHash) == -1)abortNomem();
	FILE* fp = popen(cmd, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "failed running process %s\n", cmd);
	}
	else {
		while (fgets(result, size - 1, fp) != NULL)
		{
			TerminateStrOn(result, terminators);
			if (Compare(result, "2")) {
				RES = true;
			}
		}
	}
	free(result);
	pclose(fp);
	free(cmd);
	return RES;
}

bool IsMerged(const char* repopath, const char* commithash) {
	//to check if a branch is merged, query git for all commits this reference is a parent to (ie search for all child commits)
	//if any of the child commits is a merge commit, this branch is considered merged.
	//if there are more than one child it is possible a new branch was created at this commit
	//it is possible for two children to exist: a merge commit and one non-merge commit
	//in that case I still consider THIS branch merged with a new branch being created
	//if there are childen but none of them is a merge commit, this commit could be fast forwarded in some direction but hasn't, it is not merged
	//the cut -c42- bit is to cut my own hash out of the result and only leave the children
	//git rev-list --children --all | grep ^a40691c4edac3e3e9cff1f651f79a80dd3bd792a | cut -c42- | tr ' ' '\n'
	//NOTE: the commit hash is a demo of this case, BUT it's NOT in the public GitHub version, just in the "Ground Truth" repo on a private server
	//for each check if merge commit, if at least one is a merge commit, then this branch has been merged
	const int size = 64;
	char* result = (char*)malloc(sizeof(char) * size);
	char* cmd;
	bool RES = false;
	if (asprintf(&cmd, "git -C \"%s\" rev-list --children --all | grep ^%s | cut -c42- | tr ' ' '\n'", repopath, commithash) == -1)abortNomem();
	FILE* fp = popen(cmd, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "failed running process %s\n", cmd);
	}
	else {
		while (fgets(result, size - 1, fp) != NULL)
		{
			TerminateStrOn(result, terminators);
			if (result[0] == 0x00) {
				//if the line is empty -> no need to check if it's a merge commit if it's not even a commit at all
				continue;
			}
			bool imc = IsMergeCommit(repopath, result);
			//printf("%s is%s a merge commit\n", result, imc ? "" : " NOT");
			RES = RES || imc;
		}
	}
	free(result);
	pclose(fp);
	free(cmd);
	return RES;
}

bool CheckBranching(RepoInfo* ri) {
	//this method checks the status of all branches on a given repo
	//and then computes how many differ, howm many up to date, how many branches local-only, how many branches remote-only
	BranchListSorted* ListBase = NULL;

	char* command;
	int size = 1024;
	char* result = (char*)malloc(sizeof(char) * size);
	if (asprintf(&command, "git -C \"%s\" branch -vva", ri->DirectoryPath) == -1)abortNomem();
	FILE* fp = popen(command, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "failed running process %s\n", command);
	}
	else {
		int branchcount = 0;
		while (fgets(result, size - 1, fp) != NULL)
		{
			TerminateStrOn(result, terminators);
			branchcount++;
			if (LIMIT_BRANCHES != -1 && branchcount > LIMIT_BRANCHES) {
				free(result);
				pclose(fp);
				free(command);
				fprintf(stderr, "more than %i branches -> aborting branchhandling\n", LIMIT_BRANCHES);
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
			int RegexReturnCode = regexec(&reg, result, maxGroups, CapturedResults, 0);
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
					strncpy(x, result + start, len);
					x[len] = 0x00;
					if (GroupID == 1) {
						int offs = 0;
						if (StartsWith(x, "remotes/")) {
							offs = LastIndexOf(x, '/') + 1;
							rm = true;
						}
						bname = malloc(sizeof(char) * ((len - offs) + 1));
						strncpy(bname, result + start + offs, len - offs);
						bname[len - offs] = 0x00;
					}
					else if (GroupID == 2) {
						hash = malloc(sizeof(char) * (len + 1));
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

bool CheckExtendedGitStatus(RepoInfo* ri) {
	int size = 1024;
	char* result = malloc(sizeof(char) * size);
	if (result == NULL) {
		fprintf(stderr, "OUT OF MEMORY");
		return 0;
	}
	char* command;
	if (asprintf(&command, "git -C \"%s\" status --ignore-submodules=dirty --porcelain=v2 -b --show-stash", ri->DirectoryPath) == -1)abortNomem();
	FILE* fp = popen(command, "r");
	//printf("have opened git status\r");
	if (fp == NULL)
	{
		size = 0;
		fprintf(stderr, "failed running process `%s`\n", command);
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
	return size != 0;//if I set size to 0 when erroring out, return false/0; else 1
}

void AddChild(RepoInfo* parent, RepoInfo* child) {
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
		if (asprintf(&reply, "ssh://%s", input) == -1)abortNomem();
	}
	else
	{
		if (asprintf(&reply, "%s", input) == -1)abortNomem();
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

	char* cmd;
	if (BeThorough || DoProcessWorktree) {
		if (asprintf(&cmd, "git -C \"%s\" rev-parse --is-bare-repository 2>/dev/null", ri->DirectoryPath) == -1)abortNomem();
		char* bareRes = ExecuteProcess(cmd);
		free(cmd);
		if (bareRes == NULL) {
			//Encountered error -> quit
			DeallocRepoInfoStrings(ri);
			free(ri);
			return false;
		}
		TerminateStrOn(bareRes, terminators);
		ri->isBare = Compare(bareRes, "true");
		free(bareRes);
		bareRes = NULL;
		ri->isGit = ri->isBare;//initial value

		if (!ri->isBare) {
			if (asprintf(&cmd, "git -C \"%s\" rev-parse --show-toplevel 2>/dev/null", ri->DirectoryPath) == -1)abortNomem();
			char* tlres = ExecuteProcess(cmd);
			TerminateStrOn(tlres, terminators);
			free(cmd);
			ri->isGit = Compare(ri->DirectoryPath, tlres);
			//printf("dir %s tl: %s (%i) dpw: %i\n", ri->DirectoryPath, tlres, ri->isGit, DoProcessWorktree);
			free(tlres);
			if (DoProcessWorktree && !(ri->isGit)) {
				//usually we are done at this stage (only treat as git if in toplevel, but this is for the prompt substitution where it needs to work anywhere in git)
				if (asprintf(&cmd, "git -C \"%s\" rev-parse --is-inside-work-tree 2>/dev/null", ri->DirectoryPath) == -1)abortNomem();
				char* wtres = ExecuteProcess(cmd);
				TerminateStrOn(wtres, terminators);
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
	if (asprintf(&cmd, "git -C  \"%1$s\" symbolic-ref --short HEAD 2>/dev/null || git -C \"%1$s\" describe --tags --exact-match HEAD 2>/dev/null || git -C \"%1$s\" rev-parse --short HEAD", ri->DirectoryPath) == -1)abortNomem();
	ri->branch = ExecuteProcess(cmd);
	TerminateStrOn(ri->branch, terminators);
	free(cmd);
	cmd = NULL;

	//if 'git rev-parse --show-superproject-working-tree' outputs NOTHING, a repo is standalone, if there is output it will point to the parent repo
	if (asprintf(&cmd, "git -C \"%s\" rev-parse --show-superproject-working-tree", ri->DirectoryPath) == -1)abortNomem();
	char* temp = ExecuteProcess(cmd);
	ri->parentRepo = AbbreviatePath(temp, 15, 1, 2);
	free(temp);
	temp = NULL;
	TerminateStrOn(ri->parentRepo, terminators);
	ri->isSubModule = !((ri->parentRepo)[0] == 0x00);
	free(cmd);


	CheckBranching(ri);

	if (!ri->isBare) {
		CheckExtendedGitStatus(ri);
		ri->DirtyWorktree = !(ri->ActiveMergeFiles == 0 && ri->ModifiedFiles == 0 && ri->StagedChanges == 0);
	}


	if (asprintf(&cmd, "git -C \"%s\" ls-remote --get-url origin", ri->DirectoryPath) == -1)abortNomem();
	ri->RepositoryUnprocessedOrigin = ExecuteProcess(cmd);
	free(cmd);
	if (ri->RepositoryUnprocessedOrigin == NULL) {
		DeallocRepoInfoStrings(ri);
		free(ri);
		return false;
	}
	cmd = NULL;
	TerminateStrOn(ri->RepositoryUnprocessedOrigin, terminators);
	if (Compare(ri->RepositoryUnprocessedOrigin, "origin")) {
		//if git ls-remote --get-url origin returns 'origin' it means either the folder is not a git repository OR it's a repository without remote (local only)
		//in this case since I already checked this IS a repo, it MUST be a repo without remote
		ri->HasRemote = false;
		if (asprintf(&ri->RepositoryDisplayedOrigin, "NO_REMOTE") == -1)abortNomem();
		int i = 0;
		char* tempPtr = ri->DirectoryPath;
		while (ri->DirectoryPath[i] != 0x00) {
			if (ri->DirectoryPath[i] == '/') {
				tempPtr = ri->DirectoryPath + i + 1;
			}
			i++;
		}
		if (asprintf(&ri->RepositoryName, "%s", tempPtr) == -1)abortNomem();
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
				if (asprintf(&ri->RepositoryDisplayedOrigin, "%s", NAMES[i]) == -1)abortNomem();
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
		if (asprintf(&sedCmd, "echo \"%s\" | sed -nE 's~^([-a-zA-Z0-9_]+)://(([-a-zA-Z0-9_]+)@){0,1}([-0-9a-zA-Z_\\.]+)(:([0-9]+)){0,1}([:/]([-0-9a-zA-Z_]+)){0,1}.*/([-0-9a-zA-Z_]+)(\\.git/{0,1}){0,1}$~\\1|\\3|\\4|\\6|\\8|\\9~p'", FixedProtoOrigin) == -1)abortNomem();
		//I take the capturing groups and paste them into a | seperated sting. There's 6 words (5 |), so I'll need 6 pointers into this memory area to resolve the six words
#define REPO_ORIGIN_WORDS_IN_STRING 6
#define REPO_ORIGIN_GROUP_PROTOCOL 0
#define REPO_ORIGIN_GROUP_USER 1
#define REPO_ORIGIN_GROUP_Host 2
#define REPO_ORIGIN_GROUP_PORT 3
#define REPO_ORIGIN_GROUP_GitHubUser 4 /*(if not GitHub, it's the path on remote)*/
#define REPO_ORIGIN_GROUP_RepoName 5
		char* sedRes = ExecuteProcess(sedCmd);
		TerminateStrOn(sedRes, terminators);
		if (sedRes[0] == 0x00) {//sed output was empty -> it must be a local repo, just parse the last folder as repo name and the rest as parentrepopath
			// local repo
			if (ri->RepositoryOriginID == -1)
			{
				ri->RepositoryDisplayedOrigin = AbbreviatePath(FixedProtoOrigin, 15, 2, 2);
				//if (asprintf(&ri->RepositoryDisplayedOrigin, "%s", FixedProtoOrigin) == -1)abortNomem();
			}
			char* tempptr = FixedProtoOrigin;
			char* walker = FixedProtoOrigin;
			while (*walker != 0x00) {
				if (*walker == '/') {
					tempptr = walker + 1;
				}
				walker++;
			}
			if (asprintf(&ri->RepositoryName, "%s", tempptr) == -1)abortNomem();

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
				if (asprintf(&ri->RepositoryName, "%s", ptrs[REPO_ORIGIN_GROUP_RepoName]) == -1)abortNomem();
			}

			//the rest of this only makes sense for stuff that's NOT LOCALNET/GLOBAL etc.
			if (ri->RepositoryOriginID == -1)//not a known repo origin (ie not from LOCALNET, NONE etc)
			{
				ri->RepositoryDisplayedOrigin = (char*)malloc(sizeof(char) * 256);
				int currlen = 0;
				currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_PROTOCOL], 255 - currlen);//proto
				currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ":", 255 - currlen);//:
				if (!Compare(ptrs[REPO_ORIGIN_GROUP_USER], "git") && Compare(ptrs[REPO_ORIGIN_GROUP_PROTOCOL], "ssh")) {//if name is NOT git then print it but only print if it was ssh
					currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_USER], 255 - currlen);//username
					currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, "@", 255 - currlen);//@
				}
				currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_Host], 255 - currlen);//host
				if (*ptrs[3] != 0x00) {//if port is given print it
					currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ":", 255 - currlen);//:
					currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_PORT], 255 - currlen);//username
				}
				if (*ptrs[4] != 0x00) {//host is github or gitlab and I can parse a github username also add it
					bool knownServer = false;
					int i = 0;
					while (i < numGitHubs && knownServer == false) {
						knownServer = Compare(ptrs[REPO_ORIGIN_GROUP_Host], GitHubs[i]);
						i++;
					}
					if (knownServer) {
						currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ":", 255 - currlen);//:
						currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_GitHubUser], 255 - currlen);//service username
					}
				}
			}
			for (int i = 0;i < REPO_ORIGIN_WORDS_IN_STRING;i++) {
				ptrs[i] = NULL;//to prevent UseAfterFree vulns
			}
		}
#undef REPO_ORIGIN_WORDS_IN_STRING
#undef REPO_ORIGIN_GROUP_PROTOCOL
#undef REPO_ORIGIN_GROUP_USER
#undef REPO_ORIGIN_GROUP_Host
#undef REPO_ORIGIN_GROUP_PORT
#undef REPO_ORIGIN_GROUP_GitHubUser
#undef REPO_ORIGIN_GROUP_RepoName
		free(sedRes);
		sedRes = NULL;

		//once I have the current and new repo origin IDs perform the change
		if (desiredorigin != -1 && ri->RepositoryOriginID != -1 && ri->RepositoryOriginID != desiredorigin) {

			//change
			ri->RepositoryOriginID_PREVIOUS = ri->RepositoryOriginID;
			if (ri->RepositoryUnprocessedOrigin_PREVIOUS == NULL) { free(ri->RepositoryUnprocessedOrigin_PREVIOUS); }
			ri->RepositoryUnprocessedOrigin_PREVIOUS = ri->RepositoryUnprocessedOrigin;
			ri->RepositoryOriginID = desiredorigin;
			if (asprintf(&ri->RepositoryUnprocessedOrigin, "%s/%s", LOCS[ri->RepositoryOriginID], ri->RepositoryName) == -1)abortNomem();
			char* changeCmd;
			if (asprintf(&changeCmd, "git -C \"%s\" remote set-url origin %s", ri->DirectoryPath, ri->RepositoryUnprocessedOrigin) == -1)abortNomem();
			printf("%s\n", changeCmd);
			ExecuteProcess(changeCmd);
			free(changeCmd);
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
		fprintf(stderr, "failed on directory: %s\n", directoryName);
		perror("Couldn't open the directory");
	}
	if (!BeThorough) {
		//for an explanation to this call see the top of this function where another TestPathForRepoAndParseIfExists call exists
		TestPathForRepoAndParseIfExists(ri, newRepoSpec, false, false);
	}
	return ri;
}

char* ConstructGitBranchInfoString(RepoInfo* ri) {
#define MALEN 64
	int rbLen = 0;
	char* rb = (char*)malloc(sizeof(char) * MALEN);
	rb[0] = 0x00;
	if (ri->HasRemote) { //if the repo doesn't have a remote, it doesn't make sense to count the branch differences betwen remote and local since ther is no remote
		int temp;
		if (ri->CountRemoteOnlyBranches > 0 || ri->CountLocalOnlyBranches > 0 || ri->CountUnequalBranches > 0) {
			temp = snprintf(rb + rbLen, MALEN - rbLen, ": ⟨");
			if (temp < MALEN && temp>0) {
				rbLen += temp;
			}

			if (ri->CountRemoteOnlyBranches > 0) {
				temp = snprintf(rb + rbLen, MALEN - rbLen, COLOUR_GIT_BRANCH_REMOTEONLY "%d⇣ ", ri->CountRemoteOnlyBranches);
				if (temp < MALEN && temp>0) {
					rbLen += temp;
				}
			}

			if (ri->CountLocalOnlyBranches > 0) {
				temp = snprintf(rb + rbLen, MALEN - rbLen, COLOUR_GIT_BRANCH_LOCALONLY "%d⇡ ", ri->CountLocalOnlyBranches);
				if (temp < MALEN && temp>0) {
					rbLen += temp;
				}
			}

			if (ri->CountUnequalBranches > 0) {
				temp = snprintf(rb + rbLen, MALEN - rbLen, COLOUR_GIT_BRANCH_UNEQUAL "%d⇕ ", ri->CountUnequalBranches);
				if (temp < MALEN && temp>0) {
					rbLen += temp;
				}
			}

			rb[--rbLen] = 0x00;//bin a space (after the file listings)

			temp = snprintf(rb + rbLen, MALEN - rbLen, COLOUR_GREYOUT "⟩");
			if (temp < MALEN && temp>0) {
				rbLen += temp;
			}
		}
	}
	return rb;
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
	else if (ri->CheckedOutBranchIsNotInRemote && ri->HasRemote) { //only display if there even is a remote
		//Initially I wanted not {NEW BRANCH} but {1⇡+}, where 1 is the number of commits since branching.
		//to do that I would have to start at the branch tip, look at it's parent and see how many children that has.
		//I would need to continue this chain until I find a commit with more than one child (the latest branching point) or I run out of commits (nothing on remote at all, no branches whatsoever)

		//////{
		//////	int numCommitsOnBranch = 0;
		//////	bool CheckFurter = false;
		//////	char current[64] = "HEAD";
		//////	do {
		//////		numCommitsOnBranch++;
		//////		const int size = 256;
		//////		char* result = (char*)malloc(sizeof(char) * size);
		//////		char* cmd;
		//////		bool RES = false;
		//////		//look up the commit's parent. if the parent is a branch tip on remote or the initial commit, I found the base.
		//////		//I will also have found the base if I find a commit with more than one child (one of them being me) IFF one of the OTHER children has a child somewhere down the line which itself IS a branch tip on remote
		//////		//the best way to figure this out would be to create a in-memory graph, start at all nodes that mark a remote branch tip and bfs/dfs my way through the graph, marking all parents as 'on remote'
		//////		//then start at all non-remote branch tips and bfs how many nodes I need to visit until I reach a 'on remote' node or run out of nodes
		//////		if(asprintf(&cmd, "git -C \"%s\" cat-file -p %s |grep parent|cut -c8-", ri->DirectoryPath, current) == -1)abortNomem();
		//////		int parentcount = 0;
		//////		FILE* fp = popen(cmd, "r");
		//////		if (fp == NULL)
		//////		{
		//////			fprintf(stderr, "failed running process %s\n", cmd);
		//////		}
		//////		else {
		//////			while (fgets(result, size - 1, fp) != NULL)
		//////			{
		//////				TerminateStrOn(result, terminators);
		//////				if (parentcount == 0) {
		//////					strncpy(current, result, 63);
		//////					current[63] = 0x00;
		//////					if (current[0] == 0x00) {
		//////						break;
		//////					}
		//////				}
		//////				parentcount++;
		//////			}
		//////		}
		//////		free(result);
		//////		pclose(fp);
		//////		free(cmd);
		//////		if (parentcount != 1) {
		//////			CheckFurter = false;
		//////		}
		//////	} while (CheckFurter);
		//////	printf("%s/%s has %i new commits\n", ri->RepositoryName, ri->branch, numCommitsOnBranch);
		//////}
		temp = snprintf(rb + rbLen, GIT_SEG_5_MAX_LEN - rbLen, " {" COLOUR_GIT_COMMITS "NEW BRANCH" COLOUR_CLEAR "}");
		if (temp < GIT_SEG_5_MAX_LEN && temp>0) {
			rbLen += temp;
		}
	}

	if (ri->StagedChanges > 0 || ri->ModifiedFiles > 0 || ri->UntrackedFiles > 0 || ri->ActiveMergeFiles > 0) {
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

void printTree_internal(RepoInfo* ri, const char* parentPrefix, bool anotherSameLevelEntryFollows, bool fullOut) {
	if (ri == NULL) {
		return;
	}
	if (ri->ParentDirectory != NULL) {
		printf("%s%s\u2500\u2500 ", parentPrefix, (anotherSameLevelEntryFollows ? "\u251c" : "\u2514"));
	}
	if (ri->isGit) {
		printf("%s [" COLOUR_GIT_BARE "%s" COLOUR_GIT_INDICATOR "GIT", ri->DirectoryName, ri->isBare ? "BARE " : "");
		if (ri->isSubModule) {
			printf("-SM" COLOUR_CLEAR "@" COLOUR_GIT_PARENT "%s", ri->parentRepo);
		}
		char* gitBranchInfo = ConstructGitBranchInfoString(ri);
		printf(COLOUR_CLEAR "] " COLOUR_GIT_NAME "%s" COLOUR_CLEAR " on " COLOUR_GIT_BRANCH "%s" COLOUR_GREYOUT "/%i+%i",
			ri->RepositoryName,
			ri->branch,
			ri->CountActiveBranches,
			ri->CountFullyMergedBranches);
		if (!ri->isBare) {
			printf("%s", gitBranchInfo);
		}
		printf(" " COLOUR_CLEAR "from ");
		if (gitBranchInfo != NULL)free(gitBranchInfo);

		char* GitStatStrTemp = ConstructGitStatusString(ri);
		//differentiate between display only and display after change
		if (ri->RepositoryOriginID_PREVIOUS != -1) {
			printf(COLOUR_GIT_ORIGIN "[%s(%i) -> %s(%i)]" COLOUR_CLEAR, NAMES[ri->RepositoryOriginID_PREVIOUS], ri->RepositoryOriginID_PREVIOUS, NAMES[ri->RepositoryOriginID], ri->RepositoryOriginID);
			if (!ri->isBare) {
				printf("%s", GitStatStrTemp);
			}
			if (fullOut) {
				printf(COLOUR_GREYOUT " (%s -> %s)" COLOUR_CLEAR, ri->RepositoryUnprocessedOrigin_PREVIOUS, ri->RepositoryUnprocessedOrigin);
			}
			putc('\n', stdout);
		}
		else {
			printf(COLOUR_GIT_ORIGIN "%s" COLOUR_CLEAR, ri->RepositoryDisplayedOrigin);
			if (!ri->isBare) {
				printf("%s", GitStatStrTemp);
			}
			if (fullOut) {
				printf(COLOUR_GREYOUT " (%s)" COLOUR_CLEAR, ri->RepositoryUnprocessedOrigin);
			}
			putc('\n', stdout);
		}
		free(GitStatStrTemp);

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
	if (asprintf(&temp, "%s%s", parentPrefix, (ri->ParentDirectory != NULL) ? (anotherSameLevelEntryFollows ? "\u2502   " : "    ") : "") == -1)abortNomem();
	while (current != NULL)
	{
		procedSubDirs++;
		printTree_internal(current->self, temp, procedSubDirs < ri->SubDirectoryCount, fullOut);
		current = current->next;
	}
	free(temp);
}

void printTreeBasic(RepoInfo* ri) {
	printTree_internal(ri, "", ri->SubDirectoryCount > 1, false);
}

void printTree(RepoInfo* ri, bool Detailed) {
	printTree_internal(ri, "", ri->SubDirectoryCount > 1, Detailed);
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

void Cleanup() {
	for (int i = 0;i < maxGroups;i++) {
		if (LOCS[i] != NULL) { free(LOCS[i]); };
		if (NAMES[i] != NULL) { free(NAMES[i]); };
		if (GitHubs[i] != NULL) { free(GitHubs[i]); };
	}
}

void DoSetup() {
	for (int i = 0;i < maxGroups;i++) {
		LOCS[i] = NULL;
		NAMES[i] = NULL;
		GitHubs[i] = NULL;
	}
	numLOCS = 0;
	numGitHubs = 0;
	errno = 0;

	char* file;
	char* fileName = "/repoconfig.cfg";
	char* pointerIntoEnv = getenv("ST_CFG");
	//TODO check if dir exists
	file = (char*)malloc(strlen(pointerIntoEnv) + strlen(fileName) + 1); // to account for NULL terminator
	strcpy(file, pointerIntoEnv);
	strcat(file, fileName);
	FILE* fp = fopen(file, "r");//open for read, will fail if file doesn't exist
	if (fp == NULL) {
		printf("config file didn't exist (%i: %s)\n", errno, strerror(errno));
		errno = 0;
		fp = fopen(file, "w+");//open for read/write -> create if not exists, then fill defaults, then read
		if (fp == NULL) {
			fprintf(stderr, "couldn't create file (%i: %s)\n", errno, strerror(errno));
		}
		else {
			fprintf(fp, DEFAULT_CONFIG_FILE);
			fflush(fp);
			rewind(fp);
			printf("created default config file: %s\n", file);
		}
	}
	free(file);

	int buf_max_len = 1024;
	char* buf = (char*)malloc(sizeof(char) * buf_max_len);
	if (buf == NULL) {
		printf("couldn't malloc required buffer");
	}

	//at this point I know for certain a config file does exist
	while (fgets(buf, buf_max_len - 1, fp) != NULL) {
		if (buf[0] == '#') {
			continue;
		}
		else if (StartsWith(buf, "ORIGIN:	")) {
			//found origin
			char* actbuf = buf + 8;//the 8 is the offset to just behind "ORIGIN:	"
			int i = 0;
			while (i < buf_max_len - 8 && actbuf[i] != '\t') {
				i++;
			}
			if (actbuf[i] == '\t' && i < (buf_max_len - 8 - 1)) {
				if (TerminateStrOn(actbuf + i + 1, terminators) > 0) {
					if (asprintf(&LOCS[numLOCS], "%s", actbuf + i + 1) == -1) { fprintf(stderr, "WARNING: not enough memory, provisionally continuing, be prepared!"); }
					actbuf[i] = 0x00;
					if (asprintf(&NAMES[numLOCS], "%s", actbuf) == -1) { fprintf(stderr, "WARNING: not enough memory, provisionally continuing, be prepared!"); }
					//printf("origin>%s|%s<\n", NAMES[numLOCS], LOCS[numLOCS]);
					numLOCS++;
				}
			}
		}
		else if (StartsWith(buf, "HOST:	")) {
			//found host
			if (TerminateStrOn(buf, terminators) > 0) {
				if (asprintf(&GitHubs[numGitHubs], "%s", buf + 6) == -1) { fprintf(stderr, "WARNING: not enough memory, provisionally continuing, be prepared!"); }
				//the +6 is the offset to just after "HOST:	"
				//printf("host>%s<\n", GitHubs[numGitHubs]);
				numGitHubs++;
			}
		}
		else {
			fprintf(stderr, "Warning: unknown entry in config file: >%s<", buf);
		}
	}
	free(buf);
}

uint32_t determinePossibleCombinations(int* availableLength, int NumElements, ...) {
	//this function takes a poiner to an int containing the total available size and the number of variadic arguments to be expected.
	//the variadic elements are the size of individual blocks.
	//the purpose of this function is to figure out which blocks can fit into the total size in an optimal fashion.
	//an optimal fashion means: as many as possible, but the blocks are given in descending priority.
	//example: if there's a total size of 10 and the blocks 5,7,6,3,4,2,8,1,1,1,1,1,1,1,1 the solution would be to take 5+3+2 since 5 is the most important which means there's a size of 5 left that can be filled again.
	//7 doesn't fit, so we'll take the next best thing that will fit, in this case 3, which leaves 2, which in turn can be taken by the 2.
	//if the goal was just to have "as many as possible" the example should have picked all 1es, but since I need priorities, take the first that'll fit and find the next hightest priority that'll fit
	//(which will be further back in the list, otherwise it would already have been selected)
	//this function then returns a bitfield of which blocks were selected
	assert(NumElements > 0 && NumElements <= 32);
	uint32_t res = 0;
	va_list ELEMENTS;
	va_start(ELEMENTS, NumElements);//start variadic function param handling, NumElements is the Identifier of the LAST NON-VARIADIC parameter passed to this function
	for (int i = 0; i < NumElements; i++)
	{
		//va_arg returns the next of the variadic emlements, assuming it's type is compatible with the provided one (here int)
		//if it's not compatible, it's undefined behaviour
		int nextElem = va_arg(ELEMENTS, int);
		//if the next element fits, select it and reduce the available space
		if (*availableLength > nextElem) {
			res |= 1 << i;
			*availableLength -= nextElem;
		}
	}
	va_end(ELEMENTS);//a bit like malloc/free there has to be a va_end for each va_start

	return res;
}

void ListAvailableRemotes() {
	for (int i = 0;i < numLOCS;i++) {
		printf(COLOUR_GIT_ORIGIN "%s" COLOUR_CLEAR " (-> %s)\n", NAMES[i], LOCS[i]);
	}
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
	RegexReturnCode = regcomp(&reg, RegexString, REG_EXTENDED | REG_NEWLINE);
	if (RegexReturnCode)
	{
		char* regErrorBuf = (char*)malloc(sizeof(char) * 1024);
		int elen = regerror(RegexReturnCode, &reg, regErrorBuf, 1024);
		printf("Could not compile regular expression '%s'. [%i(%s) {len:%i}]\n", RegexString, RegexReturnCode, regErrorBuf, elen);
		free(regErrorBuf);
		exit(1);
	};

	DoSetup();

	//stuff to obtain the time/date/calendarweek for prompt
	time_t tm;
	tm = time(NULL);
	struct tm* localtm = localtime(&tm);

	bool IsPrompt = 0, IsSet = 0, IsShow = 0, IsList = 0, IsLowPrompt = 0;

	bool IsThoroughSearch = 0;
	int PromptRetCode = 0;
	int Arg_TotalPromptWidth = 0;
	char* Arg_NewRemote = NULL;

	char* Arg_CmdTime = NULL;

	char* User = ExecuteProcess("whoami");
	TerminateStrOn(User, terminators);
	int User_len = strlen_visible(User);

	char* Host = ExecuteProcess("hostname");
	TerminateStrOn(Host, terminators);
	int Host_len = strlen_visible(Host);

	char* Arg_TerminalDevice = NULL;
	int Arg_TerminalDevice_len = 0;

	char* Time = malloc(sizeof(char) * 16);
	int Time_len = strftime(Time, 16, "%T", localtm);

	char* TimeZone = malloc(sizeof(char) * 17);
	int TimeZone_len = strftime(TimeZone, 17, " UTC%z (%Z)", localtm);

	char* DateInfo = malloc(sizeof(char) * 16);
	int DateInfo_len = strftime(DateInfo, 16, " %a %d.%m.%Y", localtm);

	char* CalenderWeek = malloc(sizeof(char) * 8);
	int CalenderWeek_len = strftime(CalenderWeek, 8, " KW%V", localtm);

	if (Time_len == 0 || TimeZone_len == 0 || DateInfo_len == 0 || CalenderWeek_len == 0) {
		fprintf(stderr, "WARNING: strftime returned 0 -> check format string and allocated buffer size\n");
	}

	char* Arg_LocalIPs = NULL;
	int Arg_LocalIPs_len = 0;

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

	int getopt_currentChar;//the char for getop switch

	while (1) {
		int option_index = 0;

		static struct option long_options[] = {
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

		getopt_currentChar = getopt_long(argc, argv, "b:c:d:e:fhi:j:l:n:p:qr:s:t:", long_options, &option_index);
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
				if (IsSet | IsShow | IsList) {
					printf("prompt is mutex with set|show|list|lowprompt\n");
					break;
				}
				IsPrompt = 1;
				break;
			}
		case '1':
			{
				if (IsSet | IsPrompt | IsList) {
					printf("show is mutex with set|prompt|list|lowprompt\n");
					break;
				}
				IsShow = 1;
				break;
			}
		case '2':
			{
				if (IsPrompt | IsShow | IsList) {
					printf("set is mutex with prompt|show|list|lowprompt\n");
					break;
				}
				IsSet = 1;
				break;
			}
		case '3':
			{
				if (IsPrompt | IsShow | IsSet) {
					printf("list is mutex with prompt|show|set|lowprompt\n");
					break;
				}
				IsList = 1;
				break;
			}
		case '4':
			{
				if (IsPrompt | IsShow | IsSet | IsList) {
					printf("list is mutex with prompt|show|set|list\n");
					break;
				}
				IsLowPrompt = 1;
				break;
			}
		case 'b':
			{
				TerminateStrOn(optarg, terminators);
				LIMIT_BRANCHES = atoi(optarg);
				break;
			}
		case 'c':
			{
				TerminateStrOn(optarg, terminators);
				Arg_TotalPromptWidth = atoi(optarg);
				break;
			}
		case 'd':
			{
				TerminateStrOn(optarg, terminators);
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
				TerminateStrOn(optarg, terminators);
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
				printf("%s MODE [OPTIONS] PATH\n\
\n\tMODE:\n\
\t\tprompt\n\
\t\t\tAll options EXCEPT -n are valid\n\
\t\tshow\n\
\t\t\tThe only valid Option is -b --branchlimit\n\
\t\tset\n\
", argv[0]);
				break;
			}
		case 'i':
			{
				TerminateStrOn(optarg, terminators);
				Arg_LocalIPs = optarg;
				Arg_LocalIPs_len = strlen_visible(Arg_LocalIPs);
				break;
			}
		case 'j':
			{
				TerminateStrOn(optarg, terminators);
				Arg_BackgroundJobs = optarg;
				Arg_BackgroundJobs_len = strlen_visible(Arg_BackgroundJobs);
				break;
			}
		case 'l':
			{
				TerminateStrOn(optarg, terminators);
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
				TerminateStrOn(optarg, terminators);
				Arg_NewRemote = optarg;
				break;
			}
		case 'p':
			{
				TerminateStrOn(optarg, terminators);
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
				TerminateStrOn(optarg, terminators);
				PromptRetCode = atoi(optarg);
				break;
			}
		case 's':
			{
				TerminateStrOn(optarg, terminators);
				Arg_SSHInfo = optarg;
				Arg_SSHInfo_len = strlen_visible(Arg_SSHInfo);
				break;
			}
		case 't':
			{
				TerminateStrOn(optarg, terminators);
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


#ifdef DEBUG
	printf("Arg_NewRemote: %s\n", Arg_NewRemote);fflush(stdout);
	printf("User: %s\n", User);fflush(stdout);
	printf("Host: %s\n", Host);fflush(stdout);
	printf("Arg_TerminalDevice: %s\n", Arg_TerminalDevice);fflush(stdout);
	printf("Time: %s\n", Time);fflush(stdout);
	printf("TimeZone: %s\n", TimeZone);fflush(stdout);
	printf("DateInfo: %s\n", DateInfo);fflush(stdout);
	printf("CalenderWeek: %s\n", CalenderWeek);fflush(stdout);
	printf("Arg_LocalIPs: %s\n", Arg_LocalIPs);fflush(stdout);
	printf("Arg_ProxyInfo: %s\n", Arg_ProxyInfo);fflush(stdout);
	printf("Arg_PowerState: %s\n", Arg_PowerState);fflush(stdout);
	printf("Arg_BackgroundJobs: %s\n", Arg_BackgroundJobs);fflush(stdout);
	printf("Arg_SHLVL: %s\n", Arg_SHLVL);fflush(stdout);
	printf("Arg_SSHInfo: %s\n", Arg_SSHInfo);fflush(stdout);
	for (int i = 0;i < argc;i++) {
		printf("%soption-arg %i:\t>%s<\n", (i >= optind ? "non-" : "\t"), i, argv[i]);
	}
#endif



	if (!(IsPrompt || IsShow || IsSet || IsList || IsLowPrompt)) {
		printf("you must specify EITHER --prompt --set --show --list or --lowprompt\n");
		exit(1);
	}

	if (!IsList && !(optind < argc)) {
		printf("You must supply one non-option parameter (if not in --list mode)");
	}
	char* path = argv[optind];

	if (LIMIT_BRANCHES == -2) {
		//if IsPrompt, default 25; if IsSet, default 50; else default -1
		LIMIT_BRANCHES = (IsPrompt ? 25 : (IsSet ? 50 : -1));
	}


	if (IsPrompt) //show origin info for command prompt
	{
		RepoInfo* ri = AllocRepoInfo("", path);
		//printf("Pre entering repocheck\r");
		//fflush(stdout);
		TestPathForRepoAndParseIfExists(ri, -1, true, true);
		//printf("returned out of repocheck\r");
		//fflush(stdout);
		if (ri == NULL) {
			fprintf(stderr, "error at main: TestPathForRepoAndParseIfExists returned null\n");
			return 1;
		}
		AllocUnsetStringsToEmpty(ri);

		//if top line too short -> omission order: git submodule location (git_4)(...@/path/to/parent/repo); device (/dev/pts/0); time Zone (UTC+0200 (CEST)); IP info ; git remote info(git_2) (... from displayedorigin)

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
			if (asprintf(&numBgJobsStr, "  %i Jobs", numBgJobs) == -1)abortNomem();
		}
		else {
			numBgJobsStr = (char*)malloc(sizeof(char));
			numBgJobsStr[0] = 0x00;
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
			if (asprintf(&gitSegment1_BaseMarkerStart, " [" COLOUR_GIT_BARE "%s" COLOUR_GIT_INDICATOR "GIT%s", ri->isBare ? "BARE " : "", ri->isSubModule ? "-SM" : "") == -1)abortNomem();//[%F{006}BARE %F{002}GIT-SM
			if (ri->isSubModule) {
				if (asprintf(&gitSegment2_parentRepoLoc, COLOUR_CLEAR "@" COLOUR_GIT_PARENT "%s" COLOUR_CLEAR, ri->parentRepo) == -1)abortNomem();
			}

			char* gitBranchInfo = NULL;
			if (!ri->isBare) {
				gitBranchInfo = ConstructGitBranchInfoString(ri);
			}
			else {
				gitBranchInfo = malloc(sizeof(char));
				gitBranchInfo[0] = 0x00;
			}
			if (asprintf(&gitSegment3_BaseMarkerEnd, COLOUR_CLEAR "] " COLOUR_GIT_NAME "%s" COLOUR_CLEAR " on "COLOUR_GIT_BRANCH "%s" COLOUR_GREYOUT "/%i+%i%s" COLOUR_CLEAR,
				ri->RepositoryName,
				ri->branch,
				ri->CountActiveBranches,
				ri->CountFullyMergedBranches,
				gitBranchInfo) == -1)abortNomem();
			if (gitBranchInfo != NULL)free(gitBranchInfo);
			if (asprintf(&gitSegment4_remoteinfo, " from " COLOUR_GIT_ORIGIN "%s" COLOUR_CLEAR, ri->RepositoryDisplayedOrigin) == -1)abortNomem();
			if (!ri->isBare) {
				gitSegment5_gitStatus = ConstructGitStatusString(ri);
			}
			else {
				gitSegment5_gitStatus = malloc(sizeof(char));
				gitSegment5_gitStatus[0] = 0x00;
			}

			gitSegment1_BaseMarkerStart_len = strlen_visible(gitSegment1_BaseMarkerStart);
			gitSegment2_parentRepoLoc_len = strlen_visible(gitSegment2_parentRepoLoc);
			gitSegment3_BaseMarkerEnd_len = strlen_visible(gitSegment3_BaseMarkerEnd);
			gitSegment4_remoteinfo_len = strlen_visible(gitSegment4_remoteinfo);
			gitSegment5_gitStatus_len = strlen_visible(gitSegment5_gitStatus);
		}


		int RemainingPromptWidth = Arg_TotalPromptWidth - (
			User_len + 1 + Host_len +
			Arg_SHLVL_len +
			gitSegment1_BaseMarkerStart_len +
			gitSegment3_BaseMarkerEnd_len +
			gitSegment5_gitStatus_len +
			Time_len +
			Arg_ProxyInfo_len +
			strlen_visible(numBgJobsStr) +
			Arg_PowerState_len + 1);

		uint32_t AdditionalElementAvailabilityPackedBool = determinePossibleCombinations(&RemainingPromptWidth, 9,
			gitSegment4_remoteinfo_len,
			Arg_LocalIPs_len,
			CalenderWeek_len,
			TimeZone_len,
			DateInfo_len,
			Arg_TerminalDevice_len,
			gitSegment2_parentRepoLoc_len,
			Arg_BackgroundJobs_len,
			Arg_SSHInfo_len);

#define AdditionalElementPriorityGitRemoteInfo 0
#define AdditionalElementPriorityLocalIP 1
#define AdditionalElementPriorityCalenderWeek 2
#define AdditionalElementPriorityTimeZone 3
#define AdditionalElementPriorityDate 4
#define AdditionalElementPriorityTerminalDevice 5
#define AdditionalElementPriorityParentRepoLocation 6
#define AdditionalElementPriorityBackgroundJobDetail 7
#define AdditionalElementPrioritySSHInfo 8

		//if the seventh-prioritized element (ssh connection info) has space, print it "<SSH: 123.123.13.123:54321 -> 321.312.321.321:22> "
		if (AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPrioritySSHInfo)) {
			printf("%s", Arg_SSHInfo);
		}

		//print username and machine "user@hostname"
		printf(COLOUR_USER "%s" COLOUR_USER_AT_HOST "@" COLOUR_HOST "%s" COLOUR_CLEAR, User, Host);

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
		printf("%s", gitSegment5_gitStatus);

		//fill the empty space between left and right side
		for (int i = 0;i < RemainingPromptWidth;i++) {
			printf("-");
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

		if ((AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityCalenderWeek))) {
			printf("%s", CalenderWeek);
		}

		//if a proxy is configured, show it (A=Apt, H=http(s), F=FTP, N=NoProxy) " [AHNF]"
		printf("%s", Arg_ProxyInfo);

		//if the second prioritized element (local IP addresses) has space, print it " eth0:192.168.0.2 wifi0:192.168.0.3"
		if ((AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityLocalIP))) {
			printf("%s", Arg_LocalIPs);
		}

		printf("%s", numBgJobsStr);
		//if the sixth-prioritized element (background tasks) has space, print it " {1S-  2S+}"
		if ((AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityBackgroundJobDetail))) {
			printf("%s", Arg_BackgroundJobs);
		}

		//print the battery state (the first unicode char can be any of ▲,≻,▽ or ◊[for not charging,but not discharging]), while the second unicode char indicates the presence of AC power "≻ 100% ↯"
		printf("%s", Arg_PowerState);


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
#ifndef PROFILING
		return 0; //I am unsure why there even is a special return here, I commented it out then moved it into a preproc block because it interfers with profiling
		//I think it may be to not clobber return codes from something else, but I don't know.
#endif
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
		int chars = Arg_TotalPromptWidth / 4; //allow a quarter of the screen width to be current working directory
		int segments = chars / 16; //impose a limit on how many segments make sense. if the terminal is 256 wide, chars would be 64 and I would allow 4 segements
		char* temp = AbbreviatePathAuto(path, chars, segments);
		//fprintf(stderr, "(%i %i)%s | %s\n", chars, segments, temp, path);
		printf("└%%F{cyan}%%B %s %s[%s:%i", temp, (PromptRetCode == 0 ? "%F{green}" : "%F{red}"), Arg_CmdTime, PromptRetCode);
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
		printf("]➜%%f%%b  ");
		free(temp);
		temp = NULL;
	}
	else {
		printf("unknown command %s\n", argv[1]);
		return -1;
	}
	regfree(&reg);

#ifdef PROFILING
	struct timespec ts_end;
	timespec_get(&ts_end, TIME_UTC);
	uint64_t tstart = ts_start.tv_sec * 1000ULL + (ts_start.tv_nsec / 1000000ULL);
	uint64_t tstop = ts_end.tv_sec * 1000ULL + (ts_end.tv_nsec / 1000000ULL);
	printf("\n%lu", tstop - tstart);
#endif

	return main_retcode;
}
