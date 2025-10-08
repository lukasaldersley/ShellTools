#/*
echo "$0 is library file -> skip"
exit
*/

#include "commons.h" // for ABORT_NO_MEMORY, Compare, TerminateStrOn, cpyString, ExecuteProcess_alloc, COLOUR_CLEAR, DEFAULT_TERMINATORS, StartsWith, COLOUR_GREYOUT, AbbreviatePath, ContainsString, CompareStrings, NextIndexOf, ALPHA_AFTER
#include "config.h" // for GROUPS, CONFIG_GIT_MAXBRANCHES, CONFIG_GIT_REMOTE, CONFIG_GIT_BRANCH_OVERVIEW, CONFIG_GIT_COMMIT_OVERVIEW, CONFIG_GIT_LOCALCHANGES, CONFIG_GIT_REPONAME, NAMES, CONFIG_GIT_BRANCHNAME, CONFIG_GIT_BRANCHSTATUS, LOCS, CONFIG_GI...
#include "gitfunc.h"

#include <regex.h> // for regcomp, regerror, regexec, regfree, REG_EXTENDED, REG_NEWLINE, regex_t, regmatch_t, regoff_t
#include <stdint.h> // for uint8_t
#include <stdio.h> // for NULL, asprintf, printf, snprintf, fprintf, pclose, fflush, fgets, popen, stdout, stderr, FILE, putc, size_t
#include <stdlib.h> // for free, malloc, atoi, exit
#include <string.h> // for strncpy, strlen

regmatch_t CapturedResults[maxGroups];
regex_t branchParsingRegex;

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
	if (fp == NULL) {
		fprintf(stderr, "failed running process %s\n", cmd);
	} else {
		while (fgets(result, size - 1, fp) != NULL) {
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

static bool IsMerged(const char* repopath, const char* commithash) {
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
	} else {
		if (asprintf(&cmd, "git -C \"%1$s\" rev-list --children --all --ancestry-path=%2$s | grep ^%2$s | cut -c42- | tr ' ' '\n'", repopath, commithash) == -1) ABORT_NO_MEMORY;
	}
	FILE* fp = popen(cmd, "r");
	if (fp == NULL) {
		fprintf(stderr, "failed running process %s\n", cmd);
	} else {
		while (fgets(result, size - 1, fp) != NULL) {
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
	if (ri->DirectoryName != NULL) free(ri->DirectoryName);
	if (ri->DirectoryPath != NULL) free(ri->DirectoryPath);
	if (ri->RepositoryName != NULL) free(ri->RepositoryName);
	if (ri->RepositoryDisplayedOrigin != NULL) free(ri->RepositoryDisplayedOrigin);
	if (ri->RepositoryUnprocessedOrigin != NULL) free(ri->RepositoryUnprocessedOrigin);
	if (ri->RepositoryUnprocessedOrigin_PREVIOUS != NULL) free(ri->RepositoryUnprocessedOrigin_PREVIOUS);
	if (ri->branch != NULL) free(ri->branch);
	if (ri->parentRepo != NULL) free(ri->parentRepo);
}

void AllocUnsetStringsToEmpty(RepoInfo* ri) {
	if (ri->DirectoryName == NULL) {
		ri->DirectoryName = (char*)malloc(sizeof(char));
		if (ri->DirectoryName == NULL) ABORT_NO_MEMORY;
		ri->DirectoryName[0] = 0x00;
	}
	if (ri->DirectoryPath == NULL) {
		ri->DirectoryPath = (char*)malloc(sizeof(char));
		if (ri->DirectoryPath == NULL) ABORT_NO_MEMORY;
		ri->DirectoryPath[0] = 0x00;
	}
	if (ri->RepositoryName == NULL) {
		ri->RepositoryName = (char*)malloc(sizeof(char));
		if (ri->RepositoryName == NULL) ABORT_NO_MEMORY;
		ri->RepositoryName[0] = 0x00;
	}
	if (ri->RepositoryDisplayedOrigin == NULL) {
		ri->RepositoryDisplayedOrigin = (char*)malloc(sizeof(char));
		if (ri->RepositoryDisplayedOrigin == NULL) ABORT_NO_MEMORY;
		ri->RepositoryDisplayedOrigin[0] = 0x00;
	}
	if (ri->RepositoryUnprocessedOrigin == NULL) {
		ri->RepositoryUnprocessedOrigin = (char*)malloc(sizeof(char));
		if (ri->RepositoryUnprocessedOrigin == NULL) ABORT_NO_MEMORY;
		ri->RepositoryUnprocessedOrigin[0] = 0x00;
	}
	if (ri->RepositoryUnprocessedOrigin_PREVIOUS == NULL) {
		ri->RepositoryUnprocessedOrigin_PREVIOUS = (char*)malloc(sizeof(char));
		if (ri->RepositoryUnprocessedOrigin_PREVIOUS == NULL) ABORT_NO_MEMORY;
		ri->RepositoryUnprocessedOrigin_PREVIOUS[0] = 0x00;
	}
	if (ri->branch == NULL) {
		ri->branch = (char*)malloc(sizeof(char));
		if (ri->branch == NULL) ABORT_NO_MEMORY;
		ri->branch[0] = 0x00;
	}
	if (ri->parentRepo == NULL) {
		ri->parentRepo = (char*)malloc(sizeof(char));
		if (ri->parentRepo == NULL) ABORT_NO_MEMORY;
		ri->parentRepo[0] = 0x00;
	}
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

static BranchListSorted* InsertIntoBranchListSorted(BranchListSorted* head, char* branchname, char* hash, bool remote) {
	if (head == NULL) {
		//Create Initial Element (List didn't exist previously)
		BranchListSorted* n = InitBranchListSortedElement();
		n->next = NULL;
		n->prev = NULL;
		if (asprintf(&(n->branchinfo.BranchName), "%s", branchname) == -1) ABORT_NO_MEMORY;
		if (remote) {
			if (asprintf(&(n->branchinfo.CommitHashRemote), "%s", hash) == -1) ABORT_NO_MEMORY;
		} else {
			if (asprintf(&(n->branchinfo.CommitHashLocal), "%s", hash) == -1) ABORT_NO_MEMORY;
		}
		return n;
	} else if (Compare(head->branchinfo.BranchName, branchname)) {
		//Branch Name matches, this means I know of the local copy and am adding the remote one or vice versa
		if (remote) {
			if (asprintf(&(head->branchinfo.CommitHashRemote), "%s", hash) == -1) ABORT_NO_MEMORY;
		} else {
			if (asprintf(&(head->branchinfo.CommitHashLocal), "%s", hash) == -1) ABORT_NO_MEMORY;
		}
		return head;
	} else if (CompareStrings(head->branchinfo.BranchName, branchname) == ALPHA_AFTER) {
		//The new Element is Alphabetically befory myself -> insert new element before myself
		BranchListSorted* n = InitBranchListSortedElement();
		n->next = head;
		n->prev = head->prev;
		if (head->prev != NULL) head->prev->next = n;
		head->prev = n;
		if (asprintf(&(n->branchinfo.BranchName), "%s", branchname) == -1) ABORT_NO_MEMORY;
		if (remote) {
			if (asprintf(&(n->branchinfo.CommitHashRemote), "%s", hash) == -1) ABORT_NO_MEMORY;
		} else {
			if (asprintf(&(n->branchinfo.CommitHashLocal), "%s", hash) == -1) ABORT_NO_MEMORY;
		}
		return n;
	} else if (head->next == NULL) {
		//The New Element is NOT Equal to myself and is NOT alphabetically before myself (as per the earlier checks)
		//on top of that, I AM the LAST element, so I can simply create a new last element
		BranchListSorted* n = InitBranchListSortedElement();
		n->next = NULL;
		n->prev = head;
		head->next = n;
		if (asprintf(&(n->branchinfo.BranchName), "%s", branchname) == -1) ABORT_NO_MEMORY;
		if (remote) {
			if (asprintf(&(n->branchinfo.CommitHashRemote), "%s", hash) == -1) ABORT_NO_MEMORY;
		} else {
			if (asprintf(&(n->branchinfo.CommitHashLocal), "%s", hash) == -1) ABORT_NO_MEMORY;
		}
		return head;
	} else {
		//The New Element is NOT Equal to myslef and is NOT alphabetically before myself (as per the earlier checks)
		//This time there IS another element after myself -> defer to it.
		head->next = InsertIntoBranchListSorted(head->next, branchname, hash, remote);
		return head;
	}
}

static bool CheckExtendedGitStatus(RepoInfo* ri) {
	int size = 1024;
	char* result = malloc(sizeof(char) * size);
	if (result == NULL) ABORT_NO_MEMORY;
	char* command;
	if (asprintf(&command, "git -C \"%s\" status --ignore-submodules=dirty --porcelain=v2 -b --show-stash", ri->DirectoryPath) == -1) ABORT_NO_MEMORY;
	FILE* fp = popen(command, "r");
	//printf("have opened git status\r");
	if (fp == NULL) {
		size = 0;
		fprintf(stderr, "failed running process `%s`\n", command);
	} else {
		while (fgets(result, size - 1, fp) != NULL) {
			if (result[0] == '1' || result[1] == '2') { //standard changes(add/modify/delete/...) and rename/copy
				if (result[2] != '.') {
					//staged change
					ri->StagedChanges++;
				}
				if (result[3] != '.') {
					//not staged change
					ri->ModifiedFiles++;
				}
			} else if (result[0] == '?') {
				//untracked file
				ri->UntrackedFiles++;
			} else if (result[0] == 'u') { //merge in progress
				ri->ActiveMergeFiles++;
			} else if (StartsWith(result, "# branch.ab ")) {
				//local/remote commits
				int offset = 13;
				int i = 0;
				while (result[offset + i] >= 0x30 && result[offset + i] <= 0x39) {
					i++;
				}
				result[offset + i] = 0x00;
				ri->LocalCommits = atoi(result + offset);
				ri->RemoteCommits = atoi(result + offset + i + 2);

			} else if (StartsWith(result, "# stash ")) {
				const char* num = result + 8;
				ri->stashes = atoi(num);
			}
		}
		pclose(fp);
	}
	free(result);
	free(command);
	return size != 0; //if I set size to 0 when erroring out, return false/0; else 1
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
		while (current->next != NULL) {
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

char* FixImplicitProtocol(const char* input) {
	char* reply;
	if (ContainsString(input, "@") && !ContainsString(input, "://")) {
		// repo contains @ but not :// ie it is a ssh://, but ssh:// is implicit
		//if ^(.*@[-a-zA-Z_\.0-9]+)(:[0-9]+){0,1}:([^0-9]*/.*)$ matches, the string contains one of github's idiosyncracies where they have git urls of git@github.com:username/repo.git, which do not work if you prepend ssh:// to it as ssh would treat :username as part of the host (more specifically it'd see username as port spec), if the sequence is host:port/whatever or even host:/whatever I can prepend ssh:// for clarity. in that case, replace the : with / and prepend ssh://
		//if so, replace it with ssh://$1$2/$3  group 2 in there is to support if someone has a different ssh port assigned
		uint8_t ghFixRegexGroupCount = 5;
		regmatch_t ghFixRegexGroups[ghFixRegexGroupCount];
		regex_t ghFixRegex;
		const char* ghFixRegexString = "^(.*@[-a-zA-Z_\\.0-9]+)(:[0-9]+){0,1}:([^0-9]*(/.*){0,1})$"; //the last bit with (/.*){0,1} is to allow, but not require anything after the user name on github (relevant for origin definitions)
#define GitHubFixHost	  1
#define GitHubFixPort	  2
#define GitHubFixRestRepo 3
		int ghFixRegexReturnCode;
		ghFixRegexReturnCode = regcomp(&ghFixRegex, ghFixRegexString, REG_EXTENDED | REG_NEWLINE);
		if (ghFixRegexReturnCode) {
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
				if (reply == NULL) ABORT_NO_MEMORY;
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
		} else {
			//no need to fix github's weirdness -> just simply prepend ssh://
			if (asprintf(&reply, "ssh://%s", input) == -1) ABORT_NO_MEMORY;
		}
		regfree(&ghFixRegex);
	} else {
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
			if (temp < MALEN && temp > 0) {
				rbLen += temp;
			}

			if (ri->CountRemoteOnlyBranches > 0) {
				temp = snprintf(rb + rbLen, MALEN - rbLen, COLOUR_GIT_BRANCH_REMOTEONLY "%d⇣ ", ri->CountRemoteOnlyBranches);
				if (temp < MALEN && temp > 0) {
					rbLen += temp;
				}
			}

			if (ri->CountLocalOnlyBranches > 0) {
				temp = snprintf(rb + rbLen, MALEN - rbLen, COLOUR_GIT_BRANCH_LOCALONLY "%d⇡ ", ri->CountLocalOnlyBranches);
				if (temp < MALEN && temp > 0) {
					rbLen += temp;
				}
			}

			if (ri->CountUnequalBranches > 0) {
				temp = snprintf(rb + rbLen, MALEN - rbLen, COLOUR_GIT_BRANCH_UNEQUAL "%d⇕ ", ri->CountUnequalBranches);
				if (temp < MALEN && temp > 0) {
					rbLen += temp;
				}
			}

			rb[--rbLen] = 0x00; //bin a space (after the file listings)

			temp = snprintf(rb + rbLen, MALEN - rbLen, COLOUR_GREYOUT "⟩");
			if (temp < MALEN && temp > 0) {
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
		if (temp < GIT_SEG_5_MAX_LEN && temp > 0) {
			rbLen += temp;
		}
		if (ri->stashes > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_5_MAX_LEN - rbLen, " %i#", ri->stashes);
			if (temp < GIT_SEG_5_MAX_LEN && temp > 0) {
				rbLen += temp;
			}
		}
		temp = snprintf(rb + rbLen, GIT_SEG_5_MAX_LEN - rbLen, "}");
		if (temp < GIT_SEG_5_MAX_LEN && temp > 0) {
			rbLen += temp;
		}
	} else if (ri->CheckedOutBranchIsNotInRemote && ri->HasRemote) { //only display if there even is a remote
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
		if (temp < GIT_SEG_5_MAX_LEN && temp > 0) {
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
		if (temp < GIT_SEG_6_MAX_LEN && temp > 0) {
			rbLen += temp;
		}

		if (ri->ActiveMergeFiles > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_6_MAX_LEN - rbLen, COLOUR_GIT_MERGES "%d!" COLOUR_CLEAR " ", ri->ActiveMergeFiles);
			if (temp < GIT_SEG_6_MAX_LEN && temp > 0) {
				rbLen += temp;
			}
		}

		if (ri->StagedChanges > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_6_MAX_LEN - rbLen, COLOUR_GIT_STAGED "%d+" COLOUR_CLEAR " ", ri->StagedChanges);
			if (temp < GIT_SEG_6_MAX_LEN && temp > 0) {
				rbLen += temp;
			}
		}

		if (ri->ModifiedFiles > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_6_MAX_LEN - rbLen, COLOUR_GIT_MODIFIED "%d*" COLOUR_CLEAR " ", ri->ModifiedFiles);
			if (temp < GIT_SEG_6_MAX_LEN && temp > 0) {
				rbLen += temp;
			}
		}

		if (ri->UntrackedFiles > 0) {
			temp = snprintf(rb + rbLen, GIT_SEG_6_MAX_LEN - rbLen, "%d? ", ri->UntrackedFiles);
			if (temp < GIT_SEG_6_MAX_LEN && temp > 0) {
				rbLen += temp;
			}
		}

		rb[--rbLen] = 0x00; //bin a space (after the file listings)

		temp = snprintf(rb + rbLen, GIT_SEG_6_MAX_LEN - rbLen, ">");
		if (temp < GIT_SEG_6_MAX_LEN && temp > 0) {
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
		while (current != NULL) {
			next = current->next;
			if (pruneTreeForGit(current->self)) {
				hasGitInTree = true;
			} else {
				if (current->prev != NULL) {
					current->prev->next = current->next;
				} else {
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
				printf(COLOUR_GREYOUT "/%i+%i", ri->CountActiveBranches, ri->CountFullyMergedBranches);
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
		} else {
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

	} else {
		printf(COLOUR_GREYOUT "%s" COLOUR_CLEAR "\n", ri->DirectoryName);
		//this prints the name of intermediate folders that are not git repos, but contain a repo somewhere within
		//-> those are less important->print greyed out
	}
	fflush(stdout);
	RepoList* current = ri->SubDirectories;
	int procedSubDirs = 0;
	char* temp;
	if (asprintf(&temp, "%s%s", parentPrefix, (ri->ParentDirectory != NULL) ? (anotherSameLevelEntryFollows ? "\u2502   " : "    ") : "") == -1) ABORT_NO_MEMORY;
	while (current != NULL) {
		procedSubDirs++;
		printTree_internal(current->self, temp, procedSubDirs < ri->SubDirectoryCount, fullOut);
		current = current->next;
	}
	free(temp);
}

void printTree(RepoInfo* ri, bool Detailed) {
	printTree_internal(ri, "", ri->SubDirectoryCount > 1, Detailed);
}

static bool CheckBranching(RepoInfo* ri) {
	//this method checks the status of all branches on a given repo
	//and then computes how many differ, how many up to date, how many branches local-only, how many branches remote-only
	BranchListSorted* ListBase = NULL;

	int size = 1024;
	char* result = (char*)malloc(sizeof(char) * size);
	if (result == NULL) ABORT_NO_MEMORY;
	char* command;
	if (asprintf(&command, "git -C \"%s\" branch -vva", ri->DirectoryPath) == -1) ABORT_NO_MEMORY;
	FILE* fp = popen(command, "r");
	if (fp == NULL) {
		fprintf(stderr, "failed running process %s\n", command);
	} else {
		int branchcount = 0;
		while (fgets(result, size - 1, fp) != NULL) {
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
			if (RegexReturnCode == 0) {
				char* bname = NULL;
				char* hash = NULL;
				bool rm = false;
				for (unsigned int GroupID = 0; GroupID < maxGroups; GroupID++) {
					if (CapturedResults[GroupID].rm_so == (size_t)-1) {
						break; // No more groups
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
					} else if (GroupID == 2) {
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
				} else {
					printf("found something non matching\n");
					fflush(stdout);
				}
			}
		}
	}

#ifdef PROFILING
	if (ALLOW_PROFILING_TESTPATH)
		timespec_get(&(profiling_timestamp[PROFILE_BRANCHING_HAVE_BRANCHES]), TIME_UTC);
#endif
	BranchListSorted* ptr = ListBase;
	BranchListSorted* LastKnown = NULL;
	while (ptr != NULL) {
		LastKnown = ListBase;
		if (ptr->branchinfo.CommitHashRemote != NULL) {
			ptr->branchinfo.IsMergedRemote = IsMerged(ri->DirectoryPath, ptr->branchinfo.CommitHashRemote);
		} else {
			//If the branch only exists in one place, the other shall be counted as fully merged
			//the reason is: if BOTH are fully merged I consider the branch legacy,
			//but only if BOTH are, so to make a remote - only fully merged branch count as such, the non - existing local branch must count as merged
			ptr->branchinfo.IsMergedRemote = true;
		}
		if (ptr->branchinfo.CommitHashLocal != NULL) {
			ptr->branchinfo.IsMergedLocal = IsMerged(ri->DirectoryPath, ptr->branchinfo.CommitHashLocal);
		} else {
			ptr->branchinfo.IsMergedLocal = true;
		}

		//printf("{%s: %s%c|%s%c}\n",
		//	(ptr->branchinfo.BranchName != NULL ? ptr->branchinfo.BranchName : "????"),
		//	(ptr->branchinfo.CommitHashLocal != NULL ? ptr->branchinfo.CommitHashLocal : "---"),
		//	ptr->branchinfo.IsMergedLocal ? 'M' : '!',
		//	(ptr->branchinfo.CommitHashRemote != NULL ? ptr->branchinfo.CommitHashRemote : "---"),
		//	ptr->branchinfo.IsMergedRemote ? 'M' : '!');

		if (ptr->branchinfo.CommitHashLocal == NULL && ptr->branchinfo.CommitHashRemote != NULL && !ptr->branchinfo.IsMergedRemote) { //remote-only, non-merged
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
		} else {
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
		ri->isGit = ri->isBare; //initial value

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

#ifdef PROFILING
	if (ALLOW_PROFILING_TESTPATH)
		timespec_get(&(profiling_timestamp[PROFILE_TESTPATH_BASIC_CHECKS]), TIME_UTC);
#endif

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

#ifdef PROFILING
	if (ALLOW_PROFILING_TESTPATH)
		timespec_get(&(profiling_timestamp[PROFILE_TESTPATH_WorktreeChecks]), TIME_UTC);
#endif
	if ((CONFIG_GIT_BRANCHSTATUS || CONFIG_GIT_BRANCH_OVERVIEW) && CONFIG_GIT_MAXBRANCHES > 0) {
		CheckBranching(ri);
	}
#ifdef PROFILING
	if (ALLOW_PROFILING_TESTPATH)
		timespec_get(&(profiling_timestamp[PROFILE_TESTPATH_BranchChecked]), TIME_UTC);
#endif

	if (!ri->isBare && (CONFIG_GIT_LOCALCHANGES || CONFIG_GIT_COMMIT_OVERVIEW)) {
		//if I need neither the overview over commits nor local changes, I can skip this thereby significantly speeding up the whole process
		CheckExtendedGitStatus(ri);
		ri->DirtyWorktree = !(ri->ActiveMergeFiles == 0 && ri->ModifiedFiles == 0 && ri->StagedChanges == 0);
	}
#ifdef PROFILING
	if (ALLOW_PROFILING_TESTPATH)
		timespec_get(&(profiling_timestamp[PROFILE_TESTPATH_ExtendedGitStatus]), TIME_UTC);
#endif

	if (desiredorigin != -1 || CONFIG_GIT_REMOTE || CONFIG_GIT_REPONAME) {
		//I only need to concern myself with the remote and reponame If they are either directly requested or implicitly needed for setGitBase
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
		} else {
			//has some form of remote
			ri->HasRemote = true;
			char* FixedProtoOrigin = FixImplicitProtocol(ri->RepositoryUnprocessedOrigin);

			// input: repoToTest, the path of a repo. if it is one of the defined repos, return that, if it's not, produce the short notation
			// basically this should produce the displayed name for the repo in the output buffer and additionally indicate if it's a known one
			ri->RepositoryOriginID = -1;
			for (int i = 0; i < numLOCS; i++) {
				//fprintf(stderr, "%s > %s testing against %s(%s)\n", ri->RepositoryUnprocessedOrigin, FixedProtoOrigin, LOCS[i], NAMES[i]);
				if (StartsWith(FixedProtoOrigin, LOCS[i])) {
					//fprintf(stderr, "\tSUCCESS\n");
					ri->RepositoryOriginID = i;
					if (asprintf(&ri->RepositoryDisplayedOrigin, "%s", NAMES[i]) == -1) ABORT_NO_MEMORY;
					break;
				}
			}

			char* sedCmd;
			//this regex is basically:
			//^(?<proto>[-\w+])://(?:(?<user>[-\w]+)@)?(?<host>[-\.\w]+)(?::(?<port>\d+))?(?:[:/](?<remotePath_GitHubUser>[-\w]+))?.*/(?<reponame>[-\w]+)(?:\.git/?)?$ //this is the debuggin version for regex101.com (PCRE<7.3, delimiter ~)
			//^(?<proto>[-a-zA-Z0-9_]+)://(?:(?<user>[-a-zA-Z0-9_]+)@){0,1}(?<host>[-0-9a-zA-Z_\\.]+)(?::(?<port>[0-9]+)){0,1}(?:[:/](?<remotePath_GitHubUser>[-0-9a-zA-Z_]+)){0,1}.*/(?<reponame>[-0-9a-zA-Z_]+)(?:\\.git/{0,1}){0,1}$
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
			if (sedRes[0] == 0x00) { //sed output was empty -> it must be a local repo, just parse the last folder as repo name and the rest as parentrepopath
				// local repo
				if (ri->RepositoryOriginID == -1) {
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
			} else {
				//the sed command was not empty therefore it matched as a remote thing and needs to be parsed
				char* ptrs[REPO_ORIGIN_WORDS_IN_STRING];
				ptrs[0] = sedRes;
				char* workingPointer = sedRes;
				int NextWordPointer = 0;
				while (*workingPointer != 0x00 && NextWordPointer < (REPO_ORIGIN_WORDS_IN_STRING - 1)) { //since I set NextWordPointer+1^I need to stop at WORDS-2=== 'x < (Words-1)'
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
				if (*ptrs[REPO_ORIGIN_GROUP_RepoName] != 0x00) { //repo name
					if (asprintf(&ri->RepositoryName, "%s", ptrs[REPO_ORIGIN_GROUP_RepoName]) == -1) ABORT_NO_MEMORY;
				}

				//the rest of this only makes sense for stuff that's NOT LOCALNET/GLOBAL etc.
				if (ri->RepositoryOriginID == -1) //not a known repo origin (ie not from LOCALNET, NONE etc)
				{
					const int OriginLen = 255;
					ri->RepositoryDisplayedOrigin = (char*)malloc(sizeof(char) * OriginLen + 1);
					if (ri->RepositoryDisplayedOrigin == NULL) ABORT_NO_MEMORY;
					int currlen = 0;
					currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_PROTOCOL], OriginLen - currlen); //proto
					currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ":", OriginLen - currlen); //:
					if (!Compare(ptrs[REPO_ORIGIN_GROUP_USER], "git") && Compare(ptrs[REPO_ORIGIN_GROUP_PROTOCOL], "ssh")) { //if name is NOT git then print it but only print if it was ssh
						currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_USER], OriginLen - currlen); //username
						currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, "@", OriginLen - currlen); //@
					}
					currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_Host], OriginLen - currlen); //host
					if (*ptrs[3] != 0x00) { //if port is given print it
						currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ":", OriginLen - currlen); //:
						currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_PORT], OriginLen - currlen); //username
					}
					if (*ptrs[4] != 0x00) { //host is github or gitlab and I can parse a github username also add it
						bool knownServer = false;
						int i = 0;
						while (i < numGitHubs && knownServer == false) {
							knownServer = Compare(ptrs[REPO_ORIGIN_GROUP_Host], GitHubs[i]);
							i++;
						}
						if (knownServer) {
							currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ":", OriginLen - currlen); //:
							currlen += cpyString(ri->RepositoryDisplayedOrigin + currlen, ptrs[REPO_ORIGIN_GROUP_GitHubUser], OriginLen - currlen); //service username
						}
					}
					*(ri->RepositoryDisplayedOrigin + (currlen < OriginLen ? currlen : OriginLen)) = 0x00; //ensure nullbyte
				}
				for (int i = 0; i < REPO_ORIGIN_WORDS_IN_STRING; i++) {
					ptrs[i] = NULL; //to prevent UseAfterFree vulns
				}
			}
			free(sedRes);
			sedRes = NULL;

#ifdef PROFILING
			if (ALLOW_PROFILING_TESTPATH)
				timespec_get(&(profiling_timestamp[PROFILE_TESTPATH_remote_and_reponame]), TIME_UTC);
#endif
			//once I have the current and new repo origin IDs perform the change
			//printf("INFO: desiredorigin:%i\tri->RepositoryOriginID:%i\tGROUPS[desiredorigin]:%i\tGROUPS[ri->RepositoryOriginID]:%i\n", desiredorigin, ri->RepositoryOriginID, GROUPS[desiredorigin], GROUPS[ri->RepositoryOriginID]);
			if (desiredorigin != -1 && ri->RepositoryOriginID != -1 && ri->RepositoryOriginID != desiredorigin /*all involved origins are assigned to an ORIGIN_ALIAS and new and old actually differ*/ &&
				((GROUPS[ri->RepositoryOriginID] == GROUPS[desiredorigin] && GROUPS[desiredorigin != -1]) /*GROUP CONDITION I: New and old are in the SAME group AND that group IS NOT -1*/
				 || ((GROUPS[ri->RepositoryOriginID] == 0) || GROUPS[desiredorigin] == 0))) /*OR GROUP CONDITION II: if EITHER is in the wildcard group 0, allow anyway*/
			{
				//change
				ri->RepositoryOriginID_PREVIOUS = ri->RepositoryOriginID;
				if (ri->RepositoryUnprocessedOrigin_PREVIOUS != NULL) {
					free(ri->RepositoryUnprocessedOrigin_PREVIOUS);
				}
				ri->RepositoryUnprocessedOrigin_PREVIOUS = ri->RepositoryUnprocessedOrigin;
				ri->RepositoryOriginID = desiredorigin;
				if (asprintf(&ri->RepositoryUnprocessedOrigin, "%s/%s", LOCS[ri->RepositoryOriginID], ri->RepositoryName) == -1) ABORT_NO_MEMORY;
				char* changeCmd;
				if (asprintf(&changeCmd, "git -C \"%s\" remote set-url origin %s", ri->DirectoryPath, ri->RepositoryUnprocessedOrigin) == -1) ABORT_NO_MEMORY;
				printf("%s\n", changeCmd);
				char* preventMemLeak = ExecuteProcess_alloc(changeCmd);
				if (preventMemLeak != NULL) free(preventMemLeak);
				preventMemLeak = NULL;
				free(changeCmd);
			}
		}
	}

	return true;
}

void gitfunc_init() {
	const char* RegexString = "^[ *]+([-_/0-9a-zA-Z]*) +([0-9a-fA-F]+) (\\[([-/_0-9a-zA-Z]+)\\])?.*$";
	int RegexReturnCode;
	RegexReturnCode = regcomp(&branchParsingRegex, RegexString, REG_EXTENDED | REG_NEWLINE);
	if (RegexReturnCode) {
		char* regErrorBuf = (char*)malloc(sizeof(char) * 1024);
		if (regErrorBuf == NULL) ABORT_NO_MEMORY;
		int elen = regerror(RegexReturnCode, &branchParsingRegex, regErrorBuf, 1024);
		printf("Could not compile regular expression '%s'. [%i(%s) {len:%i}]\n", RegexString, RegexReturnCode, regErrorBuf, elen);
		free(regErrorBuf);
		exit(1);
	};
}

void gitfunc_deinit() {
	regfree(&branchParsingRegex);
}
