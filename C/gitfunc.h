#ifndef __BVBS_ZSH_GITFUNC__
#define __BVBS_ZSH_GITFUNC__

#include "commons.h"

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
	bool isGitDisabled;
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

bool IsMerged(const char* repopath, const char* commithash);
void DeallocRepoInfoStrings(RepoInfo* ri);
BranchListSorted* InsertIntoBranchListSorted(BranchListSorted* head, char* branchname, char* hash, bool remote);
bool pruneTreeForGit(RepoInfo* ri);
RepoInfo* AllocRepoInfo(const char* directoryPath, const char* directoryName);
void AllocUnsetStringsToEmpty(RepoInfo* ri);
char* ConstructGitBranchInfoString(const RepoInfo* ri);
char* ConstructGitStatusString(const RepoInfo* ri);
char* ConstructCommitStatusString(const RepoInfo* ri);
char* FixImplicitProtocol(const char* input);
void AddChild(RepoInfo* parent, RepoInfo* child);
bool CheckExtendedGitStatus(RepoInfo* ri);

void printTree(RepoInfo* ri, bool Detailed);
#endif
