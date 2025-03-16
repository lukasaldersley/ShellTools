#/*
echo "$0 is library file -> skip"
exit
*/

#include "gitfunc.h"
#include <dirent.h>
#include <regex.h>

bool IsMergeCommit(const char* repoPath, const char* commitHash) {
	const int size = 32;
	char* result = (char*)malloc(sizeof(char) * size);
	if (result == NULL) ABORT_NO_MEMORY;
	char* cmd;
	bool RES = false;
	//this looks up the commit hashes for all parents of a given commit
	//usually a commit has EXACTLY ONE parent, the only exceptions are the initial commit (no parents) or a merge commit (two parents)
	//therefore if a commit has 2 parents it's a merge commit
	if (asprintf(&cmd, "git -C \"%s\" cat-file -p %s |grep parent|wc -l", repoPath, commitHash) == -1) ABORT_NO_MEMORY;
	FILE* fp = popen(cmd, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "failed running process %s\n", cmd);
	}
	else {
		while (fgets(result, size - 1, fp) != NULL)
		{
			TerminateStrOn(result, DEFAULT_TERMINATORS);
			if (Compare(result, "2")) {
				RES = true;
			}
		}
		pclose(fp);
	}
	free(result);
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
	if (result == NULL) ABORT_NO_MEMORY;
	char* cmd;
	bool RES = false;
	if (asprintf(&cmd, "git -C \"%s\" rev-list --children --all | grep ^%s | cut -c42- | tr ' ' '\n'", repopath, commithash) == -1) ABORT_NO_MEMORY;
	FILE* fp = popen(cmd, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "failed running process %s\n", cmd);
	}
	else {
		while (fgets(result, size - 1, fp) != NULL)
		{
			TerminateStrOn(result, DEFAULT_TERMINATORS);
			if (result[0] == 0x00) {
				//if the line is empty -> no need to check if it's a merge commit if it's not even a commit at all
				continue;
			}
			bool imc = IsMergeCommit(repopath, result);
			//printf("%s is%s a merge commit\n", result, imc ? "" : " NOT");
			RES = RES || imc;
		}
		pclose(fp);
	}
	free(result);
	free(cmd);
	return RES;
}


RepoInfo* AllocRepoInfo(const char* directoryPath, const char* directoryName) {
	RepoInfo* ri = (RepoInfo*)malloc(sizeof(RepoInfo));
	if (ri == NULL) ABORT_NO_MEMORY;
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

	if (asprintf(&(ri->DirectoryName), "%s", directoryName) == -1) ABORT_NO_MEMORY;
	if (asprintf(&(ri->DirectoryPath), "%s%s%s", directoryPath, ((strlen(directoryPath) > 0) ? "/" : ""), directoryName) == -1) ABORT_NO_MEMORY;
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
	if (ri->DirectoryName == NULL) { ri->DirectoryName = (char*)malloc(sizeof(char)); if (ri->DirectoryName == NULL) { ABORT_NO_MEMORY; } ri->DirectoryName[0] = 0x00; }
	if (ri->DirectoryPath == NULL) { ri->DirectoryPath = (char*)malloc(sizeof(char)); if (ri->DirectoryPath == NULL) { ABORT_NO_MEMORY; } ri->DirectoryPath[0] = 0x00; }
	if (ri->RepositoryName == NULL) { ri->RepositoryName = (char*)malloc(sizeof(char)); if (ri->RepositoryName == NULL) { ABORT_NO_MEMORY; } ri->RepositoryName[0] = 0x00; }
	if (ri->RepositoryDisplayedOrigin == NULL) { ri->RepositoryDisplayedOrigin = (char*)malloc(sizeof(char)); if (ri->RepositoryDisplayedOrigin == NULL) { ABORT_NO_MEMORY; } ri->RepositoryDisplayedOrigin[0] = 0x00; }
	if (ri->RepositoryUnprocessedOrigin == NULL) { ri->RepositoryUnprocessedOrigin = (char*)malloc(sizeof(char)); if (ri->RepositoryUnprocessedOrigin == NULL) { ABORT_NO_MEMORY; } ri->RepositoryUnprocessedOrigin[0] = 0x00; }
	if (ri->RepositoryUnprocessedOrigin_PREVIOUS == NULL) { ri->RepositoryUnprocessedOrigin_PREVIOUS = (char*)malloc(sizeof(char)); if (ri->RepositoryUnprocessedOrigin_PREVIOUS == NULL) { ABORT_NO_MEMORY; } ri->RepositoryUnprocessedOrigin_PREVIOUS[0] = 0x00; }
	if (ri->branch == NULL) { ri->branch = (char*)malloc(sizeof(char)); if (ri->branch == NULL) { ABORT_NO_MEMORY; } ri->branch[0] = 0x00; }
	if (ri->parentRepo == NULL) { ri->parentRepo = (char*)malloc(sizeof(char)); if (ri->parentRepo == NULL) { ABORT_NO_MEMORY; } ri->parentRepo[0] = 0x00; }
}

BranchListSorted* InitBranchListSortedElement() {
	BranchListSorted* a = (BranchListSorted*)malloc(sizeof(BranchListSorted));
	if (a == NULL) ABORT_NO_MEMORY;
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
		if (asprintf(&(n->branchinfo.BranchName), "%s", branchname) == -1) ABORT_NO_MEMORY;
		if (remote) {
			if (asprintf(&(n->branchinfo.CommitHashRemote), "%s", hash) == -1) ABORT_NO_MEMORY;
		}
		else {
			if (asprintf(&(n->branchinfo.CommitHashLocal), "%s", hash) == -1) ABORT_NO_MEMORY;
		}
		return n;
	}
	else if (Compare(head->branchinfo.BranchName, branchname)) {
		//Branch Name matches, this means I know of the local copy and am adding the remote one or vice versa
		if (remote) {
			if (asprintf(&(head->branchinfo.CommitHashRemote), "%s", hash) == -1) ABORT_NO_MEMORY;
		}
		else {
			if (asprintf(&(head->branchinfo.CommitHashLocal), "%s", hash) == -1) ABORT_NO_MEMORY;
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
		if (asprintf(&(n->branchinfo.BranchName), "%s", branchname) == -1) ABORT_NO_MEMORY;
		if (remote) {
			if (asprintf(&(n->branchinfo.CommitHashRemote), "%s", hash) == -1) ABORT_NO_MEMORY;
		}
		else {
			if (asprintf(&(n->branchinfo.CommitHashLocal), "%s", hash) == -1) ABORT_NO_MEMORY;
		}
		return n;
	}
	else if (head->next == NULL) {
		//The New Element is NOT Equal to myself and is NOT alphabetically before myself (as per the earlier checks)
		//on top of that, I AM the LAST element, so I can simply create a new last element
		BranchListSorted* n = InitBranchListSortedElement();
		n->next = NULL;
		n->prev = head;
		head->next = n;
		if (asprintf(&(n->branchinfo.BranchName), "%s", branchname) == -1) ABORT_NO_MEMORY;
		if (remote) {
			if (asprintf(&(n->branchinfo.CommitHashRemote), "%s", hash) == -1) ABORT_NO_MEMORY;
		}
		else {
			if (asprintf(&(n->branchinfo.CommitHashLocal), "%s", hash) == -1) ABORT_NO_MEMORY;
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

bool CheckExtendedGitStatus(RepoInfo* ri) {
	int size = 1024;
	char* result = malloc(sizeof(char) * size);
	if (result == NULL) ABORT_NO_MEMORY;
	char* command;
	if (asprintf(&command, "git -C \"%s\" status --ignore-submodules=dirty --porcelain=v2 -b --show-stash", ri->DirectoryPath) == -1) ABORT_NO_MEMORY;
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
				const char* num = result + 8;
				ri->stashes = atoi(num);
			}
		}
		pclose(fp);
	}
	free(result);
	free(command);
	return size != 0;//if I set size to 0 when erroring out, return false/0; else 1
}

void AddChild(RepoInfo* parent, RepoInfo* child) {
	//if the ParentDirectory node doesn't have any SubDirectories, it also won't have the list structure -> allocate and create it.
	if (parent->SubDirectories == NULL) {
		parent->SubDirectories = (RepoList*)malloc(sizeof(RepoList));
		if (parent->SubDirectories == NULL) ABORT_NO_MEMORY;
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
		if (current->next == NULL) ABORT_NO_MEMORY;
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
		if (asprintf(&reply, "ssh://%s", input) == -1) ABORT_NO_MEMORY;
	}
	else
	{
		if (asprintf(&reply, "%s", input) == -1) ABORT_NO_MEMORY;
	}
	return reply;
}

char* ConstructGitBranchInfoString(const RepoInfo* ri) {
#define MALEN 64
	int rbLen = 0;
	char* rb = (char*)malloc(sizeof(char) * MALEN);
	if (rb == NULL) ABORT_NO_MEMORY;
	rb[0] = 0x00;
	if (ri->HasRemote) { //if the repo doesn't have a remote, it doesn't make sense to count the branch differences betwen remote and local since ther is no remote
		if (ri->CountRemoteOnlyBranches > 0 || ri->CountLocalOnlyBranches > 0 || ri->CountUnequalBranches > 0) {
			int temp = snprintf(rb + rbLen, MALEN - rbLen, ": ⟨");
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
	rb[rbLen < MALEN ? rbLen : MALEN - 1] = 0x00; //just as 'belt and bracers' absolutely ensure there is a nullbyte in there
	return rb;
}

char* ConstructGitStatusString(const RepoInfo* ri) {
#define GIT_SEG_5_MAX_LEN 128
	int rbLen = 0;
	char* rb = (char*)malloc(sizeof(char) * GIT_SEG_5_MAX_LEN);
	if (rb == NULL) ABORT_NO_MEMORY;
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
		//////		if(asprintf(&cmd, "git -C \"%s\" cat-file -p %s |grep parent|cut -c8-", ri->DirectoryPath, current) == -1)ABORT_NO_MEMORY;
		//////		int parentcount = 0;
		//////		FILE* fp = popen(cmd, "r");
		//////		if (fp == NULL)
		//////		{
		//////			fprintf(stderr, "failed running process %s\n", cmd);
		//////		}
		//////		else {
		//////			while (fgets(result, size - 1, fp) != NULL)
		//////			{
		//////				TerminateStrOn(result, DEFAULT_TERMINATORS);
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
	rb[rbLen < GIT_SEG_5_MAX_LEN ? rbLen : GIT_SEG_5_MAX_LEN - 1] = 0x00; //just as 'belt and bracers' absolutely ensure there is a nullbyte in there
	return rb;
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
