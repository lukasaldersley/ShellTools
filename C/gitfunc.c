#/*
echo "$0 is library file -> skip"
exit
*/

#include "gitfunc.h"
#include "config.h"
#include <dirent.h>
#include <regex.h>

static bool IsMergeCommit(const char* repoPath, const char* commitHash) {
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
	//--ancestry-path was changed in git 2.38 to allow an argument. ubuntu 22.04 has only git 2.34.1 -> I cannot use this if git is older... I don't want to check it every time but may have to...
	const int size = 64;
	char* result = (char*)malloc(sizeof(char) * size);
	if (result == NULL) ABORT_NO_MEMORY;
	char* cmd;
	bool RES = false;
	if (I_HAVE_ANCIENT_GIT) {
		if (asprintf(&cmd, "git -C \"%s\" rev-list --children --all | grep ^%s | cut -c42- | tr ' ' '\n'", repopath, commithash) == -1) ABORT_NO_MEMORY;
	}
	else {
		if (asprintf(&cmd, "git -C \"%1$s\" rev-list --children --all --ancestry-path=%2$s | grep ^%2$s | cut -c42- | tr ' ' '\n'", repopath, commithash) == -1) ABORT_NO_MEMORY;
	}
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
			if (RES) {
				//if I have aleady found a merge commit why, the fuck would I continue to check everything else?
				break;
			}
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
	ri->isGitDisabled = false;
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

static BranchListSorted* InitBranchListSortedElement() {
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
		//if ^(.*@[-a-zA-Z_\.0-9]+)(:[0-9]+){0,1}:([^0-9]*/.*)$ matches, the string contains one of github's idiosyncracies where they have git urls of git@github.com:username/repo.git, which do not work if you prepend ssh:// to it as ssh would treat :username as part of the host (more specifically it'd see username as port spec), if the sequence is host:port/whatever or even host:/whatever I can prepend ssh:// for clarity. in that case, replace the : with / and prepend ssh://
		//if so, replace it with ssh://$1$2/$3  group 2 in there is to support if someone has a different ssh port assigned
		uint8_t ghFixRegexGroupCount = 5;
		regmatch_t ghFixRegexGroups[ghFixRegexGroupCount];
		regex_t ghFixRegex;
		const char* ghFixRegexString = "^(.*@[-a-zA-Z_\\.0-9]+)(:[0-9]+){0,1}:([^0-9]*(/.*){0,1})$"; //the last bit with (/.*){0,1} is to allow, but not require anything after the user name on github (relevant for origin definitions)
#define GitHubFixHost 1
#define GitHubFixPort 2
#define GitHubFixRestRepo 3
		int ghFixRegexReturnCode;
		ghFixRegexReturnCode = regcomp(&ghFixRegex, ghFixRegexString, REG_EXTENDED | REG_NEWLINE);
		if (ghFixRegexReturnCode)
		{
			char* regErrorBuf = (char*)malloc(sizeof(char) * 1024);
			if (regErrorBuf == NULL) ABORT_NO_MEMORY;
			regerror(ghFixRegexReturnCode, &ghFixRegex, regErrorBuf, 1024);
			printf("Could not compile regular expression '%s'. [%i(%s)]\n", ghFixRegexString, ghFixRegexReturnCode, regErrorBuf);
			fflush(stdout);
			free(regErrorBuf);
			exit(1);
		};

		ghFixRegexReturnCode = regexec(&ghFixRegex, input, ghFixRegexGroupCount, ghFixRegexGroups, 0);
		//man regex (3): regexec() returns zero for a successful match or REG_NOMATCH for failure.
		if (ghFixRegexReturnCode == 0) {
			int len = ghFixRegexGroups[0].rm_eo - ghFixRegexGroups[0].rm_so;
			if (len > 0) {
				uint8_t used_reply_len = 0;
				int tlen = (len + 1 + 6); //total max length is 6 for ssh:// plus the existing string length plus 1 for terminating zero
				reply = malloc(sizeof(char) * tlen);
				if (reply == NULL)ABORT_NO_MEMORY;
				reply[0] = 0x00;
				used_reply_len += snprintf(reply, tlen - (used_reply_len + 1), "ssh://");

				int ilen = ghFixRegexGroups[GitHubFixHost].rm_eo - ghFixRegexGroups[GitHubFixHost].rm_so;
				if (ilen > 0) {
					strncpy(reply + used_reply_len, input + ghFixRegexGroups[GitHubFixHost].rm_so, ilen);
					used_reply_len += ilen;
				}

				ilen = ghFixRegexGroups[GitHubFixPort].rm_eo - ghFixRegexGroups[GitHubFixPort].rm_so;
				if (ilen > 1) { //this is >1 to ensure there actually is a number following the :
					strncpy(reply + used_reply_len, input + ghFixRegexGroups[GitHubFixPort].rm_so, ilen);
					used_reply_len += ilen;
				}

				used_reply_len += snprintf(reply + used_reply_len, tlen - (used_reply_len), "/");

				ilen = ghFixRegexGroups[GitHubFixRestRepo].rm_eo - ghFixRegexGroups[GitHubFixRestRepo].rm_so;
				if (ilen > 0) {
					strncpy(reply + used_reply_len, input + ghFixRegexGroups[GitHubFixRestRepo].rm_so, ilen);
					used_reply_len += ilen;
				}
				reply[used_reply_len] = 0x00;
			}
		}
		else {
			//no need to fix github's weirdness -> just simply prepend ssh://
			if (asprintf(&reply, "ssh://%s", input) == -1) ABORT_NO_MEMORY;
		}
		regfree(&ghFixRegex);
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
			int temp = snprintf(rb + rbLen, MALEN - rbLen, " ⟨");
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

char* ConstructCommitStatusString(const RepoInfo* ri) {
#define GIT_SEG_5_MAX_LEN 64
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
	rb[rbLen < GIT_SEG_5_MAX_LEN ? rbLen : GIT_SEG_5_MAX_LEN - 1] = 0x00; //just as 'belt and bracers' absolutely ensure there is a nullbyte in there
#undef GIT_SEG_5_MAX_LEN
	return rb;
}

char* ConstructGitStatusString(const RepoInfo* ri) {
#define GIT_SEG_6_MAX_LEN 64
	int rbLen = 0;
	char* rb = (char*)malloc(sizeof(char) * GIT_SEG_6_MAX_LEN);
	if (rb == NULL) ABORT_NO_MEMORY;
	rb[0] = 0x00;

	if (ri->StagedChanges > 0 || ri->ModifiedFiles > 0 || ri->UntrackedFiles > 0 || ri->ActiveMergeFiles > 0) {
		int temp;
		temp = snprintf(rb + rbLen, GIT_SEG_6_MAX_LEN - rbLen, " <");
		if (temp < GIT_SEG_6_MAX_LEN && temp>0) {
			rbLen += temp;
		}

		if (ri->ActiveMergeFiles > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_6_MAX_LEN - rbLen, COLOUR_GIT_MERGES "%d!" COLOUR_CLEAR " ", ri->ActiveMergeFiles);
			if (temp < GIT_SEG_6_MAX_LEN && temp>0) {
				rbLen += temp;
			}
		}

		if (ri->StagedChanges > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_6_MAX_LEN - rbLen, COLOUR_GIT_STAGED "%d+" COLOUR_CLEAR " ", ri->StagedChanges);
			if (temp < GIT_SEG_6_MAX_LEN && temp>0) {
				rbLen += temp;
			}
		}

		if (ri->ModifiedFiles > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_6_MAX_LEN - rbLen, COLOUR_GIT_MODIFIED "%d*" COLOUR_CLEAR " ", ri->ModifiedFiles);
			if (temp < GIT_SEG_6_MAX_LEN && temp>0) {
				rbLen += temp;
			}
		}

		if (ri->UntrackedFiles > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_6_MAX_LEN - rbLen, "%d? ", ri->UntrackedFiles);
			if (temp < GIT_SEG_6_MAX_LEN && temp>0) {
				rbLen += temp;
			}
		}

		rb[--rbLen] = 0x00;//bin a space (after the file listings)

		temp = snprintf(rb + rbLen, GIT_SEG_6_MAX_LEN - rbLen, ">");
		if (temp < GIT_SEG_6_MAX_LEN && temp>0) {
			rbLen += temp;
		}
	}
	rb[rbLen < GIT_SEG_6_MAX_LEN ? rbLen : GIT_SEG_6_MAX_LEN - 1] = 0x00; //just as 'belt and bracers' absolutely ensure there is a nullbyte in there
#undef GIT_SEG_6_MAX_LEN
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

static void printTree_internal(RepoInfo* ri, const char* parentPrefix, bool anotherSameLevelEntryFollows, bool fullOut) {
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
			printf(COLOUR_GIT_ORIGIN "[%s(index %i, group %i) -> %s(index %i, group %i)]" COLOUR_CLEAR, NAMES[ri->RepositoryOriginID_PREVIOUS], ri->RepositoryOriginID_PREVIOUS, GROUPS[ri->RepositoryOriginID_PREVIOUS], NAMES[ri->RepositoryOriginID], ri->RepositoryOriginID, GROUPS[ri->RepositoryOriginID]);
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
