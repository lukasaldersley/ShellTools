#/*
TargetDir="$ST_CFG"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename "$0" .c).elf"
printf "compiling %s into %s/%s" "$0" "$TargetDir" "$TargetName"
SourceDirectory="$(realpath "$(dirname "$0")")"
gcc -O3 -std=c2x -Wall "$SourceDirectory/commons.c" "$SourceDirectory/inetfunc.c" "$SourceDirectory/config.c" "$SourceDirectory/gitfunc.c" "$0" -o "$TargetDir/$TargetName" "$@"
#I WANT to be able to do things like ./repotools.c -DPROFILING to add the compiler flag profiling but ALSO stuff like ./repotools "-DDEBUG -DPROFILING" to add both profiling and debug
printf " -> \e[32mDONE\e[0m(%s)\n" $?
exit
*/

#include "commons.h"
#include "config.h"
#include "inetfunc.h"
#include "gitfunc.h"
#include <regex.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <getopt.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define COLOUR_TERMINAL_DEVICE "\e[38;5;242m"
#define COLOUR_SHLVL "\e[0m"
#define COLOUR_USER "\e[38;5;010m"
#define COLOUR_USER_AT_HOST "\e[38;5;007m"
#define COLOUR_HOST "\e[38;5;033m"

#define maxGroups 10
regmatch_t CapturedResults[maxGroups];
regex_t branchParsingRegex;

typedef enum { IP_MODE_NONE, IP_MODE_LEGACY, IP_MODE_STANDALONE } IP_MODE;

#ifdef PROFILING
bool ALLOW_PROFILING_TESTPATH = false;
#define PROFILE_MAIN_ENTRY 0
#define PROFILE_MAIN_ARGS 1
#define PROFILE_CONFIG_READ 2
#define PROFILE_MAIN_COMMON_END 3
#define PROFILE_MAIN_PROMPT_ARGS_IP 4
//TestpathForRepoAndParseIfExists, requires ALLOW_PROFILING_TESTPATH to be set
#define PROFILE_MAIN_PROMPT_PATHTEST_BASE PROFILE_MAIN_PROMPT_ARGS_IP
#define PROFILE_TESTPATH_BASIC_CHECKS 5
#define PROFILE_TESTPATH_WorktreeChecks 6
#define PROFILE_BRANCHING_HAVE_BRANCHES 7
#define PROFILE_TESTPATH_BranchChecked 8
#define PROFILE_TESTPATH_ExtendedGitStatus 9
#define PROFILE_TESTPATH_remote_and_reponame 10
#define PROFILE_MAIN_PROMPT_PathGitTested 11
#define PROFILE_MAIN_PROMPT_GitBranchObtained 12
#define PROFILE_MAIN_PROMPT_CommitOverview 13
#define PROFILE_MAIN_PROMPT_GitStatus 14
#define PROFILE_MAIN_PROMPT_PREP_COMPLETE 15
#define PROFILE_MAIN_PROMPT_PRINTING_DONE 16
#define PROFILE_MAIN_SETSHOW_SETUP 17
#define PROFILE_MAIN_SETSHOW_COMPLETE 18
#define PROFILE_MAIN_END 19
#define PROFILE_COUNT 20
struct timespec profiling_timestamp[PROFILE_COUNT];
const char* profiling_label[PROFILE_COUNT] = {
	"main entry",
	"main arguments",
	"configuration load",
	"main common end",
	"main_prompt internal IP parsed",
	"TestPath: basic checks",
	"TestPath: WorktreeChecks",
	"CheckBranching: gathered baseinfo for branches",
	"TestPath: BranchChecks",
	"TestPath: Extended Git Status parsing",
	"TestPath: Remote and RemoName parsing",
	"main:prompt dir checked for git",
	"main:prompt git branch obtained",
	"main:prompt git commit overview",
	"main:prompt git status",
	"main:prompt prep complete",
	"main:prompt done",
	"main:set/show setup",
	"main:set/show done",
	"exit"
};

static double calcProfilingTime(uint8_t startIndex, uint8_t stopIndex) {
	uint64_t tstart = profiling_timestamp[startIndex].tv_sec * 1000000ULL + (profiling_timestamp[startIndex].tv_nsec / 1000ULL);
	uint64_t tstop = profiling_timestamp[stopIndex].tv_sec * 1000000ULL + (profiling_timestamp[stopIndex].tv_nsec / 1000ULL);
	return (double)(tstop - tstart) / 1000.0;
}
#endif

#define POWER_CHARGE " \e[38;5;157m▲ "
#define POWER_DISCHARGE " \e[38;5;217m▽ "
#define POWER_FULL " \e[38;5;157m≻ "
#define POWER_NOCHARGE_HIGH " \e[38;5;172m◊ "
#define POWER_NOCHARGE_LOW " \e[38;5;009m◊ "
#define POWER_UNKNOWN " \e[38;5;9m!⌧? "
#define CHARGE_AC "≈"
#define CHARGE_USB "≛"
#define CHARGE_BOTH "⩰"
#define CHARGER_CONNECTED "↯"

typedef enum { CHARGING, DISCHARGING, FULL, NOT_CHARGING, UNKNOWN } chargestate;

typedef struct {
	bool IsWSL;
	bool IsUSB;
	bool IsMains;
} PowerBitField;

static uint8_t ParsePowerSupplyEntry(const char* directory, const char* dir, PowerBitField* field, char* obuf, uint8_t avlen) {
	//	if path/dir/type==Battery
	//		path/dir/status to symbol
	//		path/dir/charging_type for fast/slow charging symbol
	//		path/dir/capacity%
	//	else if == Mains
	//		path/dir/online
	char* path;
	FILE* fp;
	uint8_t buf_max_len = 64;
	char* buf = malloc(sizeof(char) * buf_max_len + 1);
	if (buf == NULL)ABORT_NO_MEMORY;
	buf[buf_max_len] = 0x00;

	bool IsBat = false;
	bool IsUSB = false;
	bool IsMains = false;
	chargestate state = UNKNOWN;
	uint8_t len = 0;

	if (asprintf(&path, "%s/%s/type", directory, dir) == -1)ABORT_NO_MEMORY;
	fp = fopen(path, "r");
	if (fp != NULL) {
		if (fgets(buf, buf_max_len - 1, fp) != NULL) {
			TerminateStrOn(buf, DEFAULT_TERMINATORS);
			if (Compare("Battery", buf)) {
				IsBat = true;
			}
			else if (Compare("USB", buf)) {
				IsUSB = true;
			}
			else if (Compare("Mains", buf)) {
				IsMains = true;
			}
			else {
				printf("WARNING: Unknown type %s for %s/%s. Please report to ShellTools developer\n", buf, directory, dir);
			}
		}
		fclose(fp);
		fp = NULL;
	}
	if (IsBat) {
		long percent = 0;
		free(path);
		if (asprintf(&path, "%s/%s/status", directory, dir) == -1)ABORT_NO_MEMORY;
		fp = fopen(path, "r");
		if (fp != NULL) {
			if (fgets(buf, buf_max_len - 1, fp) != NULL) {
				TerminateStrOn(buf, DEFAULT_TERMINATORS);
				if (Compare(buf, "Charging")) {
					state = CHARGING;
				}
				else if (Compare(buf, "Discharging")) {
					state = DISCHARGING;
				}
				else if (Compare(buf, "Full")) {
					state = FULL;
				}
				else if (Compare(buf, "Not charging")) {
					state = NOT_CHARGING;
				}
				else if (Compare(buf, "Unknown")) {
					state = UNKNOWN;
				}
				else {
					printf("unknown status '%s' for %s/%s. Please report to ShellTools delevoper\n", buf, directory, dir);
				}
			}
			fclose(fp);
			fp = NULL;
		}
		free(path);
		if (asprintf(&path, "%s/%s/capacity", directory, dir) == -1)ABORT_NO_MEMORY;
		fp = fopen(path, "r");
		if (fp != NULL) {
			if (fgets(buf, buf_max_len - 1, fp) != NULL) {
				TerminateStrOn(buf, DEFAULT_TERMINATORS);
				percent = strtol(buf, NULL, 10);
			}
			fclose(fp);
			fp = NULL;
		}
		if (state == FULL && percent == 0 && field->IsWSL) {
			//printf("likely WSL PC without battery");
		}
		else {
			switch (state) {
			case CHARGING:
				{
					len += snprintf(obuf, avlen - len, POWER_CHARGE "%li%%\e[0m", percent);
					break;
				}
			case DISCHARGING:
				{
					len += snprintf(obuf, avlen - len, POWER_DISCHARGE "%li%%\e[0m", percent);
					break;
				}
			case FULL:
				{
					len += snprintf(obuf, avlen - len, POWER_FULL "%li%%\e[0m", percent);
					break;
				}
			case NOT_CHARGING:
				{
					if (percent >= 95) {
						len += snprintf(obuf, avlen - len, POWER_NOCHARGE_HIGH "%li%%\e[0m", percent);
					}
					else {
						len += snprintf(obuf, avlen - len, POWER_NOCHARGE_LOW "%li%%\e[0m", percent);
					}
					break;
				}
			case UNKNOWN:
				{
					len += snprintf(obuf, avlen - len, POWER_UNKNOWN "%li%%\e[0m", percent);
					break;
				}
			}
		}
	}
	else {
		free(path);
		if (asprintf(&path, "%s/%s/online", directory, dir) == -1)ABORT_NO_MEMORY;
		fp = fopen(path, "r");
		if (fp != NULL) {
			if (fgets(buf, buf_max_len - 1, fp) != NULL) {
				TerminateStrOn(buf, DEFAULT_TERMINATORS);
				bool IsOnline = Compare(buf, "1");
				field->IsMains |= (IsMains && IsOnline);
				field->IsUSB |= (IsUSB && IsOnline);
			}
			fclose(fp);
			fp = NULL;
		}
	}
	free(path);
	return len;
}

static char* GetSystemPowerState() {
#define POWER_CHARS_PER_BAT 64
#define POWER_NUM_BAT 2
#define POWER_CHARS_EXTPWR 8
	assert(POWER_CHARS_EXTPWR + POWER_CHARS_PER_BAT * POWER_NUM_BAT < UINT8_MAX);
	uint8_t powerBatMaxLen = POWER_CHARS_PER_BAT * POWER_NUM_BAT;
	uint8_t powerMaxLen = powerBatMaxLen + POWER_CHARS_EXTPWR;
	char* powerString = malloc(sizeof(char) * powerMaxLen + 1);
	if (powerString == NULL)ABORT_NO_MEMORY;
	powerString[powerMaxLen] = 0x00;
	powerString[0] = 0x00;
	uint8_t currentLen = 0;

	PowerBitField field;
	field.IsWSL = getenv("WSL_VERSION") != NULL;
	field.IsMains = false;
	field.IsUSB = false;
	DIR* directoryPointer;
	const char* path = "/sys/class/power_supply";
	directoryPointer = opendir(path);
	if (directoryPointer != NULL)
	{
		//On success, readdir() returns a pointer to a dirent structure.  (This structure may be statically allocated; do not attempt to free(3) it.)
		const struct dirent* direntptr;
		while ((direntptr = readdir(directoryPointer)))
		{
			if (/*direntptr->d_type == DT_LNK && */!Compare(direntptr->d_name, ".") && !Compare(direntptr->d_name, "..")) {
				currentLen += ParsePowerSupplyEntry(path, direntptr->d_name, &field, powerString + currentLen, powerBatMaxLen - currentLen);
			}
		}
		closedir(directoryPointer);
	}
	else
	{
		fprintf(stderr, "failed on directory: %s\n", path);
		perror("Couldn't open the directory");
	}
	if (field.IsMains || field.IsUSB) {
		if (field.IsWSL) {
			currentLen += snprintf(powerString + currentLen, powerMaxLen - currentLen, " ");
		}
		else {
			//WSL only ever reports as usb -> only report details if not WSL
			if (field.IsMains && field.IsUSB) {
				//both
				currentLen += snprintf(powerString + currentLen, powerMaxLen - currentLen, " " CHARGE_BOTH);
			}
			else if (field.IsMains) {
				//only mains
				currentLen += snprintf(powerString + currentLen, powerMaxLen - currentLen, " " CHARGE_AC);
			}
			else {
				//only USB
				currentLen += snprintf(powerString + currentLen, powerMaxLen - currentLen, " " CHARGE_USB);
			}
		}
		snprintf(powerString + currentLen, powerMaxLen - currentLen, CHARGER_CONNECTED);
	}
	//printf("PowerString: <%s> (%i/%i)", powerString, strlen(powerString), strlen_visible(powerString));
	//printf("\n");
	return powerString;
}

static bool CheckBranching(RepoInfo* ri) {
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

static bool TestPathForRepoAndParseIfExists(RepoInfo* ri, int desiredorigin, bool DoProcessWorktree, bool BeThorough) {
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

#ifdef PROFILING
			if (ALLOW_PROFILING_TESTPATH)
				timespec_get(&(profiling_timestamp[PROFILE_TESTPATH_remote_and_reponame]), TIME_UTC);
#endif
			//once I have the current and new repo origin IDs perform the change
			//printf("INFO: desiredorigin:%i\tri->RepositoryOriginID:%i\tGROUPS[desiredorigin]:%i\tGROUPS[ri->RepositoryOriginID]:%i\n", desiredorigin, ri->RepositoryOriginID, GROUPS[desiredorigin], GROUPS[ri->RepositoryOriginID]);
			if (desiredorigin != -1 && ri->RepositoryOriginID != -1 && ri->RepositoryOriginID != desiredorigin && ( /*all involved origins are assigned to an ORIGIN_ALIAS and new and old actually differ*/
				(GROUPS[ri->RepositoryOriginID] == GROUPS[desiredorigin] && GROUPS[desiredorigin != -1]) /*GROUP CONDITION I: New and old are in the SAME group AND that group IS NOT -1*/
				|| ((GROUPS[ri->RepositoryOriginID] == 0) || GROUPS[desiredorigin] == 0))) /*OR GROUP CONDITION II: if EITHER is in the wildcard group 0, allow anyway*/
			{

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

static RepoInfo* CreateDirStruct(const char* directoryPath, const char* directoryName, int newRepoSpec, bool BeThorough) {
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

static void ListAvailableRemotes() {
	for (int i = 0;i < numLOCS;i++) {
		printf(COLOUR_GIT_ORIGIN "%s" COLOUR_CLEAR " (-> %s), Group: %i\n", NAMES[i], LOCS[i], GROUPS[i]);
	}
}

int main(int argc, char** argv)
{
	printf("...\r");
	fflush(stdout);
#ifdef PROFILING
	for (int i = 0;i < PROFILE_COUNT;i++) {
		profiling_timestamp[i].tv_nsec = 0;
		profiling_timestamp[i].tv_sec = 0;
	}
	timespec_get(&(profiling_timestamp[PROFILE_MAIN_ENTRY]), TIME_UTC);
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
	struct winsize windowProps;
	//note: stdout did NOT work for LOWPROMPT and returns bogus data (I have seen 0 or mid 4 digits), stdin seems to work reliably
	ioctl(fileno(stdin), TIOCGWINSZ, &windowProps);
	int Arg_TotalPromptWidth = windowProps.ws_col;
#ifdef DEBUG
	printf("ScreenWidth: %i\n", Arg_TotalPromptWidth);
#endif
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

	IP_MODE ipMode = IP_MODE_STANDALONE;

	int getopt_currentChar;//the char for getop switch

	while (1) {
		int option_index = 0;

		const static struct option long_options[] = {
			{"branchlimit", required_argument, 0, 'b' },
			{"help", no_argument, 0, 'h' },
			{"list", no_argument, 0, '3' },
			{"lowprompt", no_argument, 0, '4' },
			{"prompt", no_argument, 0, '0' },
			{"quick", no_argument, 0, 'q'},
			{"set", no_argument, 0, '2' },
			{"show", no_argument, 0, '1' },
			{"thorough", no_argument, 0, 'f'},
			{0, 0, 0, 0 }
		};

		getopt_currentChar = getopt_long(argc, argv, "b:fhi:j:n:p:qr:t:", long_options, &option_index);
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
				if (ipMode != IP_MODE_STANDALONE) {
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
				if (Arg_LocalIPsAdditional != NULL && Arg_LocalIPsAdditional[0] != 0x00) {
					free(Arg_LocalIPsAdditional);
					Arg_LocalIPsAdditional = NULL;
				}
				if (Arg_LocalIPsAdditional == NULL) {
					Arg_LocalIPsAdditional = (char*)malloc(sizeof(char) * 1);
					if (Arg_LocalIPsAdditional == NULL) ABORT_NO_MEMORY;
					Arg_LocalIPsAdditional[0] = 0x00;
					Arg_LocalIPsAdditional_len = 0;
				}
				if (Arg_LocalIPsRoutes != NULL && Arg_LocalIPsRoutes[0] != 0x00) {
					free(Arg_LocalIPsRoutes);
					Arg_LocalIPsRoutes = NULL;
				}
				if (Arg_LocalIPsRoutes == NULL) {
					Arg_LocalIPsRoutes = (char*)malloc(sizeof(char) * 1);
					if (Arg_LocalIPsRoutes == NULL) ABORT_NO_MEMORY;
					Arg_LocalIPsRoutes[0] = 0x00;
					Arg_LocalIPsRoutes_len = 0;
				}
				break;
			}
		case 'j':
			{
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_BackgroundJobs = optarg;
				Arg_BackgroundJobs_len = strlen_visible(Arg_BackgroundJobs);
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
				printf("?? getopt returned character code 0x%2x (char: %c) with option %s ??\n", getopt_currentChar, getopt_currentChar, optarg);
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

#ifdef PROFILING
	timespec_get(&(profiling_timestamp[PROFILE_MAIN_ARGS]), TIME_UTC);
#endif

	if (IsSet || IsShow || IsPrompt || IsLowPrompt) {
		DoSetup();//this reads the config file -> as of hereI can expect to have current options
	}

#ifdef PROFILING
	timespec_get(&(profiling_timestamp[PROFILE_CONFIG_READ]), TIME_UTC);
#endif

	if (IsPrompt) {
		if (CONFIG_PROMPT_POWER) {
			Arg_PowerState = GetSystemPowerState();
			Arg_PowerState_len = strlen_visible(Arg_PowerState);
		}
		else {
			Arg_PowerState = malloc(sizeof(char));
			if (Arg_PowerState == NULL)ABORT_NO_MEMORY;
			Arg_PowerState[0] = 0x00;
			Arg_PowerState_len = 0;
		}
		if (CONFIG_PROMPT_TERMINAL_DEVICE) {
			Arg_TerminalDevice = ExecuteProcess_alloc("/usr/bin/tty");
			TerminateStrOn(Arg_TerminalDevice, DEFAULT_TERMINATORS);
			Arg_TerminalDevice_len = strlen_visible(Arg_TerminalDevice);
		}
		else {
			Arg_TerminalDevice = malloc(sizeof(char));
			if (Arg_TerminalDevice == NULL)ABORT_NO_MEMORY;
			Arg_TerminalDevice[0] = 0x00;
			Arg_TerminalDevice_len = 0;
		}
		if (!CONFIG_PROMPT_PROXY && Arg_ProxyInfo != NULL) { Arg_ProxyInfo[0] = 0x00; Arg_ProxyInfo_len = 0; }
		if (!CONFIG_PROMPT_JOBS && Arg_BackgroundJobs != NULL) { Arg_BackgroundJobs[0] = 0x00; Arg_BackgroundJobs_len = 0; }

		if (CONFIG_PROMPT_SSH) {
			char* ssh = getenv("SSH_CLIENT");
			if (ssh != NULL) {
				//echo "$SSH_CONNECTION" | sed -nE 's~^([-0-9a-zA-Z_\.:]+) ([0-9]+) ([-0-9a-zA-Z_\.:]+) ([0-9]+)$~<SSH: \1:\2 -> \3:\4> ~p'
				uint8_t SSHRegexGroupCount = 6;
				regmatch_t SSHRegexGroups[SSHRegexGroupCount];
				regex_t SSHRegex;
				const char* SSHRegexString = "^(([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)|([0-9a-fA-F:]+)) ([0-9]+) ([0-9]+)$";
#define SSHRegexIP 1
#define SSHRegexIPv4 2
#define SSHRegexIPv6 3
#define SSHRegexRemotePort 4
#define SSHRegexMyPort 5
				int SSHRegexReturnCode;
				SSHRegexReturnCode = regcomp(&SSHRegex, SSHRegexString, REG_EXTENDED | REG_NEWLINE);
				if (SSHRegexReturnCode)
				{
					char* regErrorBuf = (char*)malloc(sizeof(char) * 1024);
					if (regErrorBuf == NULL) ABORT_NO_MEMORY;
					regerror(SSHRegexReturnCode, &SSHRegex, regErrorBuf, 1024);
					printf("Could not compile regular expression '%s'. [%i(%s)]\n", SSHRegexString, SSHRegexReturnCode, regErrorBuf);
					fflush(stdout);
					free(regErrorBuf);
					exit(1);
				};

				SSHRegexReturnCode = regexec(&SSHRegex, ssh, SSHRegexGroupCount, SSHRegexGroups, 0);
				//man regex (3): regexec() returns zero for a successful match or REG_NOMATCH for failure.
				if (SSHRegexReturnCode == 0) {
					int len = SSHRegexGroups[0].rm_eo - SSHRegexGroups[0].rm_so;
					if (len > 0) {
						int tlen = (len + 1 + 20);
						Arg_SSHInfo = malloc(sizeof(char) * tlen);//"<SSH: [x]:x -> port x>", regex group 0 contains all three x plus 2 spaces -> if I add 18 I should be fine <SSH: [::1]:55450 -> port 22>
						if (Arg_SSHInfo == NULL)ABORT_NO_MEMORY;
						Arg_SSHInfo_len = 0;
						Arg_SSHInfo[0] = 0x00;
						Arg_SSHInfo_len += snprintf(Arg_SSHInfo, tlen - (Arg_SSHInfo_len + 1), "<SSH: ");

						int ilen = SSHRegexGroups[SSHRegexIPv4].rm_eo - SSHRegexGroups[SSHRegexIPv4].rm_so;
						if (ilen > 0) {
							strncpy(Arg_SSHInfo + Arg_SSHInfo_len, ssh + SSHRegexGroups[SSHRegexIPv4].rm_so, ilen);
							Arg_SSHInfo_len += ilen;
						}
						else {
							ilen = SSHRegexGroups[SSHRegexIPv6].rm_eo - SSHRegexGroups[SSHRegexIPv6].rm_so;
							if (ilen > 0) {
								Arg_SSHInfo_len += snprintf(Arg_SSHInfo + Arg_SSHInfo_len, tlen - (Arg_SSHInfo_len), "[");
								strncpy(Arg_SSHInfo + Arg_SSHInfo_len, ssh + SSHRegexGroups[SSHRegexIPv6].rm_so, ilen);
								Arg_SSHInfo_len += ilen;
								Arg_SSHInfo_len += snprintf(Arg_SSHInfo + Arg_SSHInfo_len, tlen - (Arg_SSHInfo_len), "]");
							}
						}
						ilen = SSHRegexGroups[SSHRegexRemotePort].rm_eo - SSHRegexGroups[SSHRegexRemotePort].rm_so;
						if (ilen > 0) {
							Arg_SSHInfo_len += snprintf(Arg_SSHInfo + Arg_SSHInfo_len, tlen - (Arg_SSHInfo_len), ":");
							strncpy(Arg_SSHInfo + Arg_SSHInfo_len, ssh + SSHRegexGroups[SSHRegexRemotePort].rm_so, ilen);
							Arg_SSHInfo_len += ilen;
						}
						ilen = SSHRegexGroups[SSHRegexMyPort].rm_eo - SSHRegexGroups[SSHRegexMyPort].rm_so;
						if (ilen > 0) {
							Arg_SSHInfo_len += snprintf(Arg_SSHInfo + Arg_SSHInfo_len, tlen - (Arg_SSHInfo_len), " -> port ");
							strncpy(Arg_SSHInfo + Arg_SSHInfo_len, ssh + SSHRegexGroups[SSHRegexMyPort].rm_so, ilen);
							Arg_SSHInfo_len += ilen;
						}
						Arg_SSHInfo_len += snprintf(Arg_SSHInfo + Arg_SSHInfo_len, tlen - (Arg_SSHInfo_len), "> ");
					}
				}
				regfree(&SSHRegex);
			}
		}
		if (Arg_SSHInfo == NULL) {
			Arg_SSHInfo = malloc(sizeof(char));
			if (Arg_SSHInfo == NULL)ABORT_NO_MEMORY;
			Arg_SSHInfo[0] = 0x00;
			Arg_SSHInfo_len = 0;
		}
		fflush(stdout);

		const char* lvl = getenv("SHLVL");
		if (!Compare("1", lvl)) {
			if (asprintf(&Arg_SHLVL, " [%s]", lvl) == -1)ABORT_NO_MEMORY;
			Arg_SHLVL_len = strlen_visible(Arg_SHLVL);
		}
		else {
			Arg_SHLVL = malloc(sizeof(char));
			if (Arg_SHLVL == NULL)ABORT_NO_MEMORY;
			Arg_SHLVL[0] = 0x00;
			Arg_SHLVL_len = 0;
		}

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
	}

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

#ifdef DEBUG
	for (int i = 0;i < argc;i++) {
		printf("%soption-arg %i:\t>%s<\n", (i >= optind ? "non-" : "\t"), i, argv[i]);
	}
	fflush(stdout);

#endif

#ifdef PROFILING
	timespec_get(&(profiling_timestamp[PROFILE_MAIN_COMMON_END]), TIME_UTC);
#endif

	if (IsPrompt) //show origin info for command prompt
	{
		printf("   \n");
		//this is intentionally not OR-ed with IsPrompt as IsPrompt is in an exhaustive if/else where if this would evaluate to false I would get an error to the effect of "unknown option PROMPT"
		if (CONFIG_PROMPT_OVERALL_ENABLE) {
			if (ipMode == IP_MODE_STANDALONE && CONFIG_PROMPT_NETWORK) {
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
#ifdef PROFILING
			timespec_get(&(profiling_timestamp[PROFILE_MAIN_PROMPT_ARGS_IP]), TIME_UTC);
#endif

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
			printf("Workpath: >%s<\n", path);fflush(stdout);
#endif
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

#ifdef PROFILING
			ALLOW_PROFILING_TESTPATH = true;
#endif
			printf("[ShellTools] Waiting for git in directory %s\r", path);
			fflush(stdout);
			RepoInfo* ri = AllocRepoInfo("", path);
			//only test git if git is enabled at all
			if (CONFIG_PROMPT_GIT) {
				char* overridefile;
				if (asprintf(&overridefile, "%s/forcegit", getenv("ST_CFG")) == -1)ABORT_NO_MEMORY;

				bool hasOverride = (access(overridefile, F_OK) != -1);
				bool allowTesting = true;
				if (hasOverride == false) {//I only need to check the exclusions if I don't have an override
					for (int i = 0;i < numGitExclusions;i++) {
						if (StartsWith(path, GIT_EXCLUSIONS[i])) {
							allowTesting = false;
							break;
						}
					}
				}
				else {
					//I have an override
					if (CONFIG_GIT_AUTO_RESTORE_EXCLUSION) {
						//override needs to be reset
						remove(overridefile);
					}
				}
				free(overridefile);
				overridefile = NULL;
				if (allowTesting) {
					if (!TestPathForRepoAndParseIfExists(ri, -1, true, true)) {
						//if TestPathForRepoAndParseIfExists fails it'll do it's own cleanup (= deallocation etc)
						fprintf(stderr, "error at main: TestPathForRepoAndParseIfExists returned null\n");
						return 1;
					}
				}
				else {
					ri->isGitDisabled = true;
				}
			}
			AllocUnsetStringsToEmpty(ri);

#ifdef PROFILING
			ALLOW_PROFILING_TESTPATH = false;
			timespec_get(&(profiling_timestamp[PROFILE_MAIN_PROMPT_PathGitTested]), TIME_UTC);
#endif

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

			if (CONFIG_PROMPT_GIT) {
				if (ri->isGitDisabled) {
					if (asprintf(&gitSegment1_BaseMarkerStart, " [GIT DISABLED]") == -1) ABORT_NO_MEMORY;
				}
				else if (ri->isGit) {
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
#ifdef PROFILING
					timespec_get(&(profiling_timestamp[PROFILE_MAIN_PROMPT_GitBranchObtained]), TIME_UTC);
#endif
					if (!ri->isBare) {
						if (CONFIG_GIT_COMMIT_OVERVIEW) {
							gitSegment5_commitStatus = ConstructCommitStatusString(ri);
						}
#ifdef PROFILING
						timespec_get(&(profiling_timestamp[PROFILE_MAIN_PROMPT_CommitOverview]), TIME_UTC);
#endif
						if (CONFIG_GIT_LOCALCHANGES) {
							gitSegment6_gitStatus = ConstructGitStatusString(ri);
						}
#ifdef PROFILING
						timespec_get(&(profiling_timestamp[PROFILE_MAIN_PROMPT_GitStatus]), TIME_UTC);
#endif
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
				Arg_TerminalDevice_len + 1,/*NOTE: the +1 is the : needed infront of /dev/whatever, but it isn't in here yet*/
				gitSegment2_parentRepoLoc_len,
				Arg_BackgroundJobs_len,
				Arg_LocalIPsAdditional_len,
				Arg_LocalIPsRoutes_len,
				Arg_SSHInfo_len);

#ifdef PROFILING
			timespec_get(&(profiling_timestamp[PROFILE_MAIN_PROMPT_PREP_COMPLETE]), TIME_UTC);
#endif

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
				printf(COLOUR_TERMINAL_DEVICE ":%s", Arg_TerminalDevice);
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
				//this switch statement is only responsible for printing possibly neccessary escape chars
				switch (CONFIG_PROMPT_FILLER_CHAR[0]) {
				case '\\': {
						putc(CONFIG_PROMPT_FILLER_CHAR[0], stdout);
						putc(CONFIG_PROMPT_FILLER_CHAR[0], stdout);
						//intentionally missing break; as \ needs more escaping to work
					}
				case '%':
					{
						//since % is an escape char for the prompt string, escape it as %%
						putc(CONFIG_PROMPT_FILLER_CHAR[0], stdout);
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
				printf("%s", CONFIG_PROMPT_FILLER_CHAR);
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
			printf("%s\n", ~AdditionalElementAvailabilityPackedBool & ~(~0 << AdditionalElementCount) ? " !" : "");

#ifdef PROFILING
			timespec_get(&(profiling_timestamp[PROFILE_MAIN_PROMPT_PRINTING_DONE]), TIME_UTC);
#endif

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
			if (Arg_SHLVL != NULL) {
				free(Arg_SHLVL);
				Arg_SHLVL = NULL;
			}
			free(Arg_SSHInfo);
			free(Arg_PowerState);
			free(Arg_TerminalDevice);
			free(Time);
			Time = NULL;
			free(TimeZone);
			TimeZone = NULL;
			free(DateInfo);
			DateInfo = NULL;
			free(CalendarWeek);
			CalendarWeek = NULL;
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

#ifdef PROFILING
		timespec_get(&(profiling_timestamp[PROFILE_MAIN_SETSHOW_SETUP]), TIME_UTC);
#endif

		RepoInfo* treeroot = CreateDirStruct("", path, RequestedNewOriginID, IsThoroughSearch);
		pruneTreeForGit(treeroot);
		if (RequestedNewOriginID != -1) {
			printf("\n");
		}

#ifdef PROFILING
		timespec_get(&(profiling_timestamp[PROFILE_MAIN_SETSHOW_COMPLETE]), TIME_UTC);
#endif
		printTree(treeroot, IsThoroughSearch);
		printf("\n");
	}
	else if (IsList) {
		ListAvailableRemotes();
		return 0;
	}
	else if (IsLowPrompt) {
		//once again a single unicode char and escape sequence -> mark as escape sequence for ZSH with %{...%} and add %G to signify glitch (the unicode char)
		printf("%%{%%G%s%%}%%{\e[36m\e[1m %%}", CONFIG_LOWPROMPT_START_CHAR);
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
					chars = CONFIG_LOWPROMPT_PATH_MAXLEN < 0 ? 0 : CONFIG_LOWPROMPT_PATH_MAXLEN;
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
		printf("%%{%s %%}", (PromptRetCode == 0 ? "\e[32m" : "\e[31m"));
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
		//escape sequence and a singe unicode char -> treat as single char
		printf("%%{%%G%s%%}%%{\e[0m  %%}", CONFIG_LOWPROMPT_END_CHAR);
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
	timespec_get(&(profiling_timestamp[PROFILE_MAIN_END]), TIME_UTC);
	uint8_t lastNonNull = 0;
	for (int i = 1;i < PROFILE_MAIN_END + 1;i++) {
		if (profiling_timestamp[i].tv_nsec == 0 && profiling_timestamp[i].tv_sec == 0) {
			continue;
		}
		else {
			printf("\n(index %2i->%2i) %s:\t%.3lfms", lastNonNull, i, profiling_label[i], calcProfilingTime(lastNonNull, i));
			if (i == PROFILE_MAIN_PROMPT_PathGitTested) {
				printf("\n(index %2i->%2i) [%s->%s]:\t%.3lfms", PROFILE_MAIN_PROMPT_PATHTEST_BASE, i, profiling_label[PROFILE_MAIN_PROMPT_PATHTEST_BASE], profiling_label[i], calcProfilingTime(PROFILE_MAIN_PROMPT_PATHTEST_BASE, i));
			}
			lastNonNull = i;
		}
	}

	printf("\ntotal:\t%.3lfms%s\n", calcProfilingTime(PROFILE_MAIN_ENTRY, lastNonNull), IsLowPrompt ? " -> " : "");
#endif

	return main_retcode;
}
