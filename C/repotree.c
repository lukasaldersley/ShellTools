#/*
TargetDir="$ST_CFG"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename "$0" .c).elf"
printf "compiling %s into %s/%s" "$0" "$TargetDir" "$TargetName"
SourceDirectory="$(realpath "$(dirname "$0")")"
gcc -O3 -std=c2x -Wall "$SourceDirectory/commons.c" "$SourceDirectory/config.c" "$SourceDirectory/gitfunc.c" "$0" -o "$TargetDir/$TargetName" "$@"
#I WANT to be able to do things like ./shelltoolsmain.c -DPROFILING to add the compiler flag profiling but ALSO stuff like ./shelltoolsmain "-DDEBUG -DPROFILING" to add both profiling and debug
printf " -> \e[32mDONE\e[0m(%s)\n" $?
exit
*/

#include "commons.h" // for Compare, TerminateStrOn, COLOUR_CLEAR, DEFAULT_TERMINATORS
#include "config.h" // for CONFIG_GIT_MAXBRANCHES, NAMES, Cleanup, DoSetup, numLOCS, CONFIG_GIT_BRANCHNAME, CONFIG_GIT_BRANCHSTATUS, CONFIG_GIT_BRANCH_OVERVIEW, CONFIG_GIT_COMMIT_OVERVIEW, CONFIG_GIT_LOCALCHANGES, CONFIG_GIT_REMOTE, CONFIG_GIT_REPONAME
#include "gitfunc.h" // for TestPathForRepoAndParseIfExists, RepoInfo, AddChild, AllocRepoInfo, COLOUR_GIT_ORIGIN, gitfunc_deinit, gitfunc_init, printTree, pruneTreeForGit

#include <dirent.h> // for dirent, DT_DIR, closedir, opendir, readdir, DIR
#include <getopt.h> // for no_argument, option, getopt_long, required_argument
#include <stdbool.h> // for false, bool, true
#include <stdio.h> // for printf, fprintf, perror, NULL, stderr
#include <stdlib.h> // for atoi, exit
#include <unistd.h> // for optarg, optind, NULL

static void ListAvailableRemotes() {
	for (int i = 0; i < numLOCS; i++) {
		printf(COLOUR_GIT_ORIGIN "%s" COLOUR_CLEAR " (-> %s), Group: %i\n", NAMES[i], LOCS[i], GROUPS[i]);
	}
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
	if (directoryPointer != NULL) {
		while ((direntptr = readdir(directoryPointer))) {
			//printf("testing: %s (directory: %s)\n", direntptr->d_name, (direntptr->d_type == DT_DIR ? "TRUE" : "NO"));
			if (!BeThorough && Compare(direntptr->d_name, ".git")) {
				ri->isGit = true;
			}
			if (direntptr->d_type != DT_DIR || Compare(direntptr->d_name, ".") || Compare(direntptr->d_name, "..")) {
				//if the current file isn't a directory I needn't check for subdirectories
				continue;
			} else if (direntptr->d_type == DT_DIR && !ri->isBare) {
				AddChild(ri, CreateDirStruct(ri->DirectoryPath, direntptr->d_name, newRepoSpec, BeThorough));
			}
		}
		closedir(directoryPointer);
	} else {
		fprintf(stderr, "failed on directory: %s\n", directoryName);
		perror("Couldn't open the directory");
	}
	if (!BeThorough) {
		//for an explanation to this call see the top of this function where another TestPathForRepoAndParseIfExists call exists
		TestPathForRepoAndParseIfExists(ri, newRepoSpec, false, false);
	}
	return ri;
}

int main(int argc, char** argv) {
	gitfunc_init();
	bool IsSet = 0, IsShow = 0, IsList = 0;
	bool IsThoroughSearch = 0;

	const char* Arg_NewRemote = NULL;

	int getopt_currentChar; //the char for getop switch
	int option_index = 0;

	const static struct option long_options[] = {
		{"branchlimit", required_argument, 0, 'b'},
		{"help", no_argument, 0, 'h'},
		{"list", no_argument, 0, '3'},
		{"quick", no_argument, 0, 'q'},
		{"set", no_argument, 0, '2'},
		{"show", no_argument, 0, '1'},
		{"thorough", no_argument, 0, 'f'},
		{0, 0, 0, 0},
	};

	while (1) {

		getopt_currentChar = getopt_long(argc, argv, "b:fhn:q", long_options, &option_index);
		if (getopt_currentChar == -1)
			break;

		switch (getopt_currentChar) {
			case 0: {
				printf("long option %s", long_options[option_index].name);
				if (optarg)
					printf(" with arg %s", optarg);
				printf("\n");
				break;
			}
			case '1': {
				if (IsSet || IsList) {
					printf("show is mutex with set|list\n");
					break;
				}
				IsShow = 1;
				break;
			}
			case '2': {
				if (IsShow || IsList) {
					printf("set is mutex with show|list\n");
					break;
				}
				IsSet = 1;
				break;
			}
			case '3': {
				if (IsShow || IsSet) {
					printf("list is mutex with show|set\n");
					break;
				}
				IsList = 1;
				break;
			}
			case 'b': {
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				CONFIG_GIT_MAXBRANCHES = atoi(optarg);
				break;
			}
			case 'f': {
				IsThoroughSearch = 1;
				break;
			}
			case 'h': {
				printf("TODO: create a help utility\n");
				break;
			}
			case 'n': {
				//printf("option n: %s", optarg);fflush(stdout);
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_NewRemote = optarg;
				break;
			}
			case 'q': {
				IsThoroughSearch = false;
				break;
			}
			case '?': {
				printf("option %c: >%s<\n", getopt_currentChar, optarg);
				break;
			}
			default: {
				printf("?? getopt returned character code 0x%2x (char: %c) with option %s ??\n", getopt_currentChar, getopt_currentChar, optarg);
			}
		}
	}

	if (!(IsShow || IsSet || IsList)) {
		printf("you must specify EITHER --set --show or --list\n");
		exit(1);
	}

	if (!IsList && !(optind < argc)) {
		printf("You must supply one non-option parameter (if not in --list mode)");
	}
	const char* path = argv[optind];

	DoSetup(); //this reads the config file -> as of hereI can expect to have current options

	//same as in shelltoolsmain this sets common/general settings flags to what the current instance needs (shelltoolsmain sets what is required for prompt, this sets for tree-based ops)
	//the -2 here is set as the default value in config.c and simply indicates this has not been overwritten by a commandline optin handled by getopt
	//if it's -2 aka unspecified, apply the config-file value, else don't touch it as it's a cmd argument therefore higher prio.
	if (CONFIG_GIT_MAXBRANCHES == -2) {
		//if IsPrompt, default 25; if IsSet, default 50; else default -1
		CONFIG_GIT_MAXBRANCHES = (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_BRANCHLIMIT : CONFIG_LSGIT_QUICK_BRANCHLIMIT);
	}
	CONFIG_GIT_WARN_BRANCH_LIMIT = CONFIG_LSGIT_WARN_BRANCHLIMIT;

	CONFIG_GIT_REPOTYPE = (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_REPOTYPE : CONFIG_LSGIT_QUICK_REPOTYPE);
	CONFIG_GIT_REPOTYPE_PARENT = (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_REPOTYPE_PARENT : CONFIG_LSGIT_QUICK_REPOTYPE_PARENT);
	CONFIG_GIT_REPONAME = (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_REPONAME : CONFIG_LSGIT_QUICK_REPONAME);
	CONFIG_GIT_BRANCHNAME = (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_BRANCHNAME : CONFIG_LSGIT_QUICK_BRANCHNAME);
	CONFIG_GIT_BRANCH_OVERVIEW = (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_BRANCHINFO : CONFIG_LSGIT_QUICK_BRANCHINFO);
	CONFIG_GIT_BRANCHSTATUS = (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_BRANCHSTATUS : CONFIG_LSGIT_QUICK_BRANCHSTATUS);
	CONFIG_GIT_REMOTE = (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_REMOTE : CONFIG_LSGIT_QUICK_REMOTE);
	CONFIG_GIT_COMMIT_OVERVIEW = (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_COMMITS : CONFIG_LSGIT_QUICK_COMMITS);
	CONFIG_GIT_LOCALCHANGES = (IsThoroughSearch ? CONFIG_LSGIT_THOROUGH_GITSTATUS : CONFIG_LSGIT_QUICK_GITSTATUS);

	if (IsSet || IsShow) {

		int RequestedNewOriginID = -1;
		if (IsSet) { //a change was requested
			for (int i = 0; i < numLOCS; i++) {
				if (Compare(Arg_NewRemote, NAMES[i])) { //found requested new origin
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
	} else if (IsList) {
		ListAvailableRemotes();
		return 0;
	} else {
		printf("unknown command %s\n", argv[1]);
		return -1;
	}
	if (IsSet || IsShow) {
		Cleanup();
	}
	gitfunc_deinit();
}
