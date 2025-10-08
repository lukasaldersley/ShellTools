#/*
echo "$0 is library file -> skip"
exit
*/

#include "commons.h" // for StartsWith, Compare, ABORT_NO_MEMORY, ParseCharOrCodePoint, TerminateStrOn, DEFAULT_TERMINATORS
#include "config.h"
#include "gitfunc.h" // for FixImplicitProtocol

#include <errno.h> // for errno
#include <regex.h> // for regcomp, regerror, regexec, regfree, REG_EXTENDED, REG_NEWLINE, regex_t, regmatch_t
#include <stdio.h> // for fprintf, NULL, stderr, printf, asprintf, fclose, fopen, fflush, fgets, FILE, rewind, stdout
#include <stdlib.h> // for free, malloc, atoi, getenv, exit, strtol
#include <string.h> // for strerror, strlen, strncpy, strcat, strcpy

char* NAMES[MaxLocations];
char* LOCS[MaxLocations];
int8_t GROUPS[MaxLocations];
char* GitHubs[MaxLocations];
uint8_t numGitHubs = 0;
uint8_t numLOCS = 0;
bool CONFIG_DISABLE_LOCS_CHECKING = false;

char* GIT_EXCLUSIONS[MaxLocations];
uint8_t numGitExclusions;

bool CONFIG_GIT_AUTO_RESTORE_EXCLUSION = true;

bool CONFIG_LOWPROMPT_INDICATE_VENV = true;
bool CONFIG_LOWPROMPT_PATH_LIMIT = true;
int CONFIG_LOWPROMPT_PATH_MAXLEN = -3;
bool CONFIG_LOWPROMPT_RETCODE = true;
bool CONFIG_LOWPROMPT_RETCODE_DECODE = true;
bool CONFIG_LOWPROMPT_TIMER = true;
char CONFIG_LOWPROMPT_START_CHAR[5];
char CONFIG_LOWPROMPT_END_CHAR[5];

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
char CONFIG_PROMPT_FILLER_CHAR[5];

bool CONFIG_PROMPT_NET_IFACE = true;
bool CONFIG_PROMPT_NET_ADDITIONAL = true;
bool CONFIG_PROMPT_NET_ROUTE = true;
bool CONFIG_PROMPT_NET_LINKSPEED = true;

//this is designed to serve as a marker to indicate no value has been read from command-line.
//all of the options in this block are the common ones that get populated at runtime either with the set of options for prompt,
//or those for one of the lsgit-flavors. -> the -2 will never actually land at execution as if it's -2 it will always be overwritten.
//shelltoolsmain doesn't actually check for that and blindly overwrites it since at that point arguments make no sense.
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
int CONFIG_LSGIT_QUICK_BRANCHLIMIT = 10;
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
int CONFIG_LSGIT_THOROUGH_BRANCHLIMIT = -1;
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
bool I_HAVE_ANCIENT_GIT = false;

void DoSetup() {
	//default filler is '-' (U+002D)
	CONFIG_PROMPT_FILLER_CHAR[0] = '-';
	CONFIG_PROMPT_FILLER_CHAR[1] = 0x00;
	//default lowprompt char is '⮱' (U+2BB1)
	CONFIG_LOWPROMPT_START_CHAR[0] = 0xE2;
	CONFIG_LOWPROMPT_START_CHAR[1] = 0xAE;
	CONFIG_LOWPROMPT_START_CHAR[2] = 0xB1;
	CONFIG_LOWPROMPT_START_CHAR[3] = 0x00;
	//default lowprompt char is '➜' (U+279C)
	CONFIG_LOWPROMPT_END_CHAR[0] = 0xE2;
	CONFIG_LOWPROMPT_END_CHAR[1] = 0x9E;
	CONFIG_LOWPROMPT_END_CHAR[2] = 0x9C;
	CONFIG_LOWPROMPT_END_CHAR[3] = 0x00;
	for (int i = 0; i < MaxLocations; i++) {
		LOCS[i] = NULL;
		NAMES[i] = NULL;
		GROUPS[i] = -1;
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
	FILE* fp = fopen(configFilePath, "r"); //open for read, will fail if configFilePath doesn't exist
	if (fp == NULL) {
		printf("config file didn't exist (%i: %s)\n", errno, strerror(errno));
		errno = 0;
		fp = fopen(configFilePath, "w+"); //open for read/write -> create if not exists, then fill defaults, then read
		if (fp == NULL) {
			fprintf(stderr, "couldn't create file (%i: %s)\n", errno, strerror(errno));
			free(configFilePath);
			free(buf);
			return;
		} else {
			fprintf(fp, "###\n###THIS FILE IS *NOT* AUTOMATICALLY UPDATED AFTER INITIAL CREATION\n###CHECK THE TEMPLATE FILE AT $ST_SRC/DEFAULTCONFIG.cfg FOR POSSIBLE NEW OPTIONS\n###\n");
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
			} else {
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

	uint8_t ConfigRegexGroupCount = 16;
	regmatch_t ConfigRegexGroups[ConfigRegexGroupCount];
	regex_t ConfigRegex;
	const char* ConfigRegexString = "^ORIGIN_ALIAS:\t([^\t]+)\t([^\t]+)(\t([0-9]+))?$";
#define ConfigRegexNAME	 1
#define ConfigRegexURL	 2
#define ConfigRegexGROUP 4
	int ConfigRegexReturnCode;
	ConfigRegexReturnCode = regcomp(&ConfigRegex, ConfigRegexString, REG_EXTENDED | REG_NEWLINE);
	if (ConfigRegexReturnCode) {
		char* regErrorBuf = (char*)malloc(sizeof(char) * 1024);
		if (regErrorBuf == NULL) ABORT_NO_MEMORY;
		regerror(ConfigRegexReturnCode, &ConfigRegex, regErrorBuf, 1024);
		printf("Could not compile regular expression '%s'. [%i(%s)]\n", ConfigRegexString, ConfigRegexReturnCode, regErrorBuf);
		fflush(stdout);
		free(regErrorBuf);
		exit(1);
	};

	bool UnknownConfig = false;
	//at this point I know for certain a config file does exist
	while (fgets(buf, buf_max_len - 1, fp) != NULL) {
		if (buf[0] == '#') {
			continue;
		} else {
			if (TerminateStrOn(buf, DEFAULT_TERMINATORS) == 0) {
				continue;
			}

			ConfigRegexReturnCode = regexec(&ConfigRegex, buf, ConfigRegexGroupCount, ConfigRegexGroups, 0);
			//man regex (3): regexec() returns zero for a successful match or REG_NOMATCH for failure.
			if (ConfigRegexReturnCode == 0) {
				if (numLOCS >= (MaxLocations - 1)) {
					fprintf(stderr, "WARNING: YOU HAVE CONFIGURED MORE THAN %1$i ORIGIN_ALIAS ENTRIES. ONLY THE FIRST %1$i WILL BE USED\n", MaxLocations);
					continue;
				}
				int len = ConfigRegexGroups[ConfigRegexURL].rm_eo - ConfigRegexGroups[ConfigRegexURL].rm_so;
				if (len > 0) {
					//run fiximplicitProtocol and warn if there's differences. (do it after everything in config has been read to facilitate disabling via config)
					LOCS[numLOCS] = malloc(sizeof(char) * (len + 1));
					if (LOCS[numLOCS] == NULL) ABORT_NO_MEMORY;
					strncpy(LOCS[numLOCS], buf + ConfigRegexGroups[ConfigRegexURL].rm_so, len);
					LOCS[numLOCS][len] = 0x00;
				}
				len = ConfigRegexGroups[ConfigRegexNAME].rm_eo - ConfigRegexGroups[ConfigRegexNAME].rm_so;
				if (len > 0) {
					NAMES[numLOCS] = malloc(sizeof(char) * (len + 1));
					if (NAMES[numLOCS] == NULL) ABORT_NO_MEMORY;
					strncpy(NAMES[numLOCS], buf + ConfigRegexGroups[ConfigRegexNAME].rm_so, len);
					NAMES[numLOCS][len] = 0x00;
				}
				len = ConfigRegexGroups[ConfigRegexGROUP].rm_eo - ConfigRegexGroups[ConfigRegexGROUP].rm_so;
				if (len > 0) {
					GROUPS[numLOCS] = strtol(buf + ConfigRegexGroups[ConfigRegexGROUP].rm_so, NULL, 10);
				}
#ifdef DEBUG
				printf("origin>%s|%s|%i<\n", NAMES[numLOCS], LOCS[numLOCS], GROUPS[numLOCS]);
#endif
				numLOCS++;

			}

			else if (StartsWith(buf, "DISABLE_ORIGIN_ALIAS_ERROR_CHECKING:	")) {
				CONFIG_DISABLE_LOCS_CHECKING = Compare("true", buf + 37);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 37, CONFIG_DISABLE_LOCS_CHECKING);
#endif
			} else if (StartsWith(buf, "GITHUB_HOST:	")) {
				if (numGitHubs >= (MaxLocations - 1)) {
					fprintf(stderr, "WARNING: YOU HAVE CONFIGURED MORE THAN %1$i GITHUB_HOST ENTRIES. ONLY THE FIRST %1$i WILL BE USED\n", MaxLocations);
					continue;
				}
				//found host
				if (asprintf(&GitHubs[numGitHubs], "%s", buf + 13) == -1) {
					fprintf(stderr, "WARNING: not enough memory, provisionally continuing, be prepared!");
					continue;
				} else {
#ifdef DEBUG
					printf("host>%s<\n", GitHubs[numGitHubs]);
#endif
					numGitHubs++;
				}
				//the +13 is the offset to just after "GITHUB_HOST:	"
			} else if (StartsWith(buf, "GIT_EXCLUSION:	")) {
				if (numGitExclusions >= (MaxLocations - 1)) {
					fprintf(stderr, "WARNING: YOU HAVE CONFIGURED MORE THAN %1$i GIT_EXCLUSION ENTRIES. ONLY THE FIRST %1$i WILL BE USED\n", MaxLocations);
					continue;
				}
				//found host
				if (asprintf(&GIT_EXCLUSIONS[numGitExclusions], "%s", buf + 15) == -1) {
					fprintf(stderr, "WARNING: not enough memory, provisionally continuing, be prepared!");
					continue;
				} else {
#ifdef DEBUG
					printf("git-exclusions>%s<\n", GIT_EXCLUSIONS[numGitExclusions]);
#endif
					numGitExclusions++;
				}
			} else if (StartsWith(buf, "SHELLTOOLS.GIT.AUTO_RESTORE_EXCLUSION:	")) {
				CONFIG_GIT_AUTO_RESTORE_EXCLUSION = Compare("true", buf + 39);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 39, CONFIG_GIT_AUTO_RESTORE_EXCLUSION);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.QUICK.MAXBRANCHES:	")) {
				CONFIG_LSGIT_QUICK_BRANCHLIMIT = atoi(buf + 36);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 36, CONFIG_LSGIT_QUICK_BRANCHLIMIT);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.THOROUGH.MAXBRANCHES:	")) {
				CONFIG_LSGIT_THOROUGH_BRANCHLIMIT = atoi(buf + 39);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 39, CONFIG_LSGIT_THOROUGH_BRANCHLIMIT);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.WARN_BRANCH_LIMIT:	")) {
				CONFIG_LSGIT_WARN_BRANCHLIMIT = Compare("true", buf + 36);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 36, CONFIG_LSGIT_WARN_BRANCHLIMIT);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LOWPROMPT.VENV.ENABLE:	")) {
				CONFIG_LOWPROMPT_INDICATE_VENV = Compare("true", buf + 34);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 34, CONFIG_LOWPROMPT_INDICATE_VENV);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LOWPROMPT.PATH.LIMIT_DISPLAY_LENGTH.ENABLE:	")) {
				CONFIG_LOWPROMPT_PATH_LIMIT = Compare("true", buf + 55);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 55, CONFIG_LOWPROMPT_PATH_LIMIT);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LOWPROMPT.PATH.LIMIT_DISPLAY_LENGTH.TARGET:	")) {
				CONFIG_LOWPROMPT_PATH_MAXLEN = atoi(buf + 55);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 55, CONFIG_LOWPROMPT_PATH_MAXLEN);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LOWPROMPT.RETURNCODE.ENABLE:	")) {
				CONFIG_LOWPROMPT_RETCODE = Compare("true", buf + 40);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 40, CONFIG_LOWPROMPT_RETCODE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LOWPROMPT.RETURNCODE.DECODE.ENABLE:	")) {
				CONFIG_LOWPROMPT_RETCODE_DECODE = Compare("true", buf + 47);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 47, CONFIG_LOWPROMPT_RETCODE_DECODE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LOWPROMPT.COMMAND_TIMER.ENABLE:	")) {
				CONFIG_LOWPROMPT_TIMER = Compare("true", buf + 43);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 43, CONFIG_LOWPROMPT_TIMER);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LOWPROMPT.START_CHAR:	")) {
				ParseCharOrCodePoint(buf + 33, CONFIG_LOWPROMPT_START_CHAR);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> '%s'\n", buf, buf + 33, CONFIG_LOWPROMPT_START_CHAR);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LOWPROMPT.END_CHAR:	")) {
				ParseCharOrCodePoint(buf + 31, CONFIG_LOWPROMPT_END_CHAR);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> '%s'\n", buf, buf + 31, CONFIG_LOWPROMPT_END_CHAR);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.SSHINFO.ENABLE:	")) {
				CONFIG_PROMPT_SSH = Compare("true", buf + 34);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 34, CONFIG_PROMPT_SSH);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.TERMINALDEVICE.ENABLE:	")) {
				CONFIG_PROMPT_TERMINAL_DEVICE = Compare("true", buf + 41);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 41, CONFIG_PROMPT_TERMINAL_DEVICE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.TIME.ENABLE:	")) {
				CONFIG_PROMPT_TIME = Compare("true", buf + 31);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 31, CONFIG_PROMPT_TIME);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.TIMEZONE.ENABLE:	")) {
				CONFIG_PROMPT_TIMEZONE = Compare("true", buf + 35);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 35, CONFIG_PROMPT_TIMEZONE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.DATE.ENABLE:	")) {
				CONFIG_PROMPT_DATE = Compare("true", buf + 31);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 31, CONFIG_PROMPT_DATE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.CALENDARWEEK.ENABLE:	")) {
				CONFIG_PROMPT_CALENDARWEEK = Compare("true", buf + 39);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 39, CONFIG_PROMPT_CALENDARWEEK);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.PROXYSTATUS.ENABLE:	")) {
				CONFIG_PROMPT_PROXY = Compare("true", buf + 38);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 38, CONFIG_PROMPT_PROXY);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.NETWORK.ENABLE:	")) {
				CONFIG_PROMPT_NETWORK = Compare("true", buf + 34);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 34, CONFIG_PROMPT_NETWORK);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.BACKGROUNDJOBS.ENABLE:	")) {
				CONFIG_PROMPT_JOBS = Compare("true", buf + 41);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 41, CONFIG_PROMPT_JOBS);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.POWER.ENABLE:	")) {
				CONFIG_PROMPT_POWER = Compare("true", buf + 32);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 32, CONFIG_PROMPT_POWER);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.ENABLE:	")) {
				CONFIG_PROMPT_OVERALL_ENABLE = Compare("true", buf + 26);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 26, CONFIG_PROMPT_OVERALL_ENABLE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.GIT.ENABLE:	")) {
				CONFIG_PROMPT_GIT = Compare("true", buf + 30);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 30, CONFIG_PROMPT_GIT);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.NETWORK.INTERFACES.DEFAULT.ENABLE:	")) {
				CONFIG_PROMPT_NET_IFACE = Compare("true", buf + 53);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 53, CONFIG_PROMPT_NET_IFACE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.NETWORK.INTERFACES.NONDEFAULT.ENABLE:	")) {
				CONFIG_PROMPT_NET_ADDITIONAL = Compare("true", buf + 56);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 56, CONFIG_PROMPT_NET_ADDITIONAL);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.NETWORK.ROUTINGINFO.ENABLE:	")) {
				CONFIG_PROMPT_NET_ROUTE = Compare("true", buf + 46);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 46, CONFIG_PROMPT_NET_ROUTE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.NETWORK.LINKSPEED.ENABLE:	")) {
				CONFIG_PROMPT_NET_LINKSPEED = Compare("true", buf + 44);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 44, CONFIG_PROMPT_NET_LINKSPEED);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.GIT.REPOTYPE.ENABLE:	")) {
				CONFIG_PROMPT_GIT_REPOTYPE = Compare("true", buf + 39);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 39, CONFIG_PROMPT_GIT_REPOTYPE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.GIT.REPOTYPE.PARENT_REPO.ENABLE:	")) {
				CONFIG_PROMPT_GIT_REPOTYPE_PARENT = Compare("true", buf + 51);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 51, CONFIG_PROMPT_GIT_REPOTYPE_PARENT);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.GIT.COMMIT_OVERVIEW.ENABLE:	")) {
				CONFIG_PROMPT_GIT_COMMITS = Compare("true", buf + 46);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 46, CONFIG_PROMPT_GIT_COMMITS);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.QUICK.REPOTYPE.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_REPOTYPE = Compare("true", buf + 40);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 40, CONFIG_LSGIT_QUICK_REPOTYPE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.QUICK.REPOTYPE.PARENT_REPO.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_REPOTYPE_PARENT = Compare("true", buf + 52);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 52, CONFIG_LSGIT_QUICK_REPOTYPE_PARENT);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.QUICK.REPONAME.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_REPONAME = Compare("true", buf + 40);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 40, CONFIG_LSGIT_QUICK_REPONAME);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.QUICK.BRANCH.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_BRANCHNAME = Compare("true", buf + 38);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 38, CONFIG_LSGIT_QUICK_BRANCHNAME);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.QUICK.BRANCH.OVERVIEW.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_BRANCHINFO = Compare("true", buf + 47);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 47, CONFIG_LSGIT_QUICK_BRANCHINFO);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.QUICK.REMOTE.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_REMOTE = Compare("true", buf + 38);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 38, CONFIG_LSGIT_QUICK_REMOTE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.QUICK.COMMIT_OVERVIEW.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_COMMITS = Compare("true", buf + 47);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 47, CONFIG_LSGIT_QUICK_COMMITS);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.QUICK.LOCALCHANGES.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_GITSTATUS = Compare("true", buf + 44);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 44, CONFIG_LSGIT_QUICK_GITSTATUS);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.THOROUGH.REPOTYPE.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_REPOTYPE = Compare("true", buf + 43);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 43, CONFIG_LSGIT_THOROUGH_REPOTYPE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.THOROUGH.REPOTYPE.PARENT_REPO.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_REPOTYPE_PARENT = Compare("true", buf + 55);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 55, CONFIG_LSGIT_THOROUGH_REPOTYPE_PARENT);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.THOROUGH.REPONAME.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_REPONAME = Compare("true", buf + 43);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 43, CONFIG_LSGIT_THOROUGH_REPONAME);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.THOROUGH.BRANCH.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_BRANCHNAME = Compare("true", buf + 41);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 41, CONFIG_LSGIT_THOROUGH_BRANCHNAME);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.THOROUGH.BRANCH.OVERVIEW.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_BRANCHINFO = Compare("true", buf + 50);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 50, CONFIG_LSGIT_THOROUGH_BRANCHINFO);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.THOROUGH.REMOTE.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_REMOTE = Compare("true", buf + 41);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 41, CONFIG_LSGIT_THOROUGH_REMOTE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.THOROUGH.COMMIT_OVERVIEW.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_COMMITS = Compare("true", buf + 50);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 50, CONFIG_LSGIT_THOROUGH_COMMITS);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.THOROUGH.LOCALCHANGES.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_GITSTATUS = Compare("true", buf + 47);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 47, CONFIG_LSGIT_THOROUGH_GITSTATUS);
#endif
			}

			else if (StartsWith(buf, "SHELLTOOLS.PROMPT.GIT.REMOTE.ENABLE:	")) {
				CONFIG_PROMPT_GIT_REMOTE = Compare("true", buf + 37);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 37, CONFIG_PROMPT_GIT_REMOTE);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.GIT.BRANCH.OVERVIEW.ENABLE:	")) {
				CONFIG_PROMPT_GIT_BRANCHINFO = Compare("true", buf + 46);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 46, CONFIG_PROMPT_GIT_BRANCHINFO);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.GIT.BRANCHSTATUS.ENABLE:	")) {
				CONFIG_PROMPT_GIT_BRANCHSTATUS = Compare("true", buf + 43);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 43, CONFIG_PROMPT_GIT_BRANCHSTATUS);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.QUICK.BRANCHSTATUS.ENABLE:	")) {
				CONFIG_LSGIT_QUICK_BRANCHSTATUS = Compare("true", buf + 44);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 44, CONFIG_LSGIT_QUICK_BRANCHSTATUS);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.LSGIT.THOROUGH.BRANCHSTATUS.ENABLE:	")) {
				CONFIG_LSGIT_THOROUGH_BRANCHSTATUS = Compare("true", buf + 47);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 47, CONFIG_LSGIT_THOROUGH_BRANCHSTATUS);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.GIT.BRANCH.ENABLE:	")) {
				CONFIG_PROMPT_GIT_BRANCHNAME = Compare("true", buf + 37);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 37, CONFIG_PROMPT_GIT_BRANCHNAME);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.GIT.REPONAME.ENABLE:	")) {
				CONFIG_PROMPT_GIT_REPONAME = Compare("true", buf + 39);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 39, CONFIG_PROMPT_GIT_REPONAME);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.GIT.LOCALCHANGES.ENABLE:	")) {
				CONFIG_PROMPT_GIT_GITSTATUS = Compare("true", buf + 43);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 43, CONFIG_PROMPT_GIT_GITSTATUS);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.GIT.MAXBRANCHES:	")) {
				CONFIG_PROMPT_GIT_BRANCHLIMIT = atoi(buf + 35);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 35, CONFIG_PROMPT_GIT_BRANCHLIMIT);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.GIT.WARN_BRANCH_LIMIT:	")) {
				CONFIG_PROMPT_GIT_WARN_BRANCHLIMIT = Compare("true", buf + 41);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 41, CONFIG_PROMPT_GIT_WARN_BRANCHLIMIT);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.USER.ENABLE:	")) {
				CONFIG_PROMPT_USER = Compare("true", buf + 31);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 31, CONFIG_PROMPT_USER);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.HOST.ENABLE:	")) {
				CONFIG_PROMPT_HOST = Compare("true", buf + 31);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> %i\n", buf, buf + 31, CONFIG_PROMPT_HOST);
#endif
			} else if (StartsWith(buf, "SHELLTOOLS.PROMPT.FILLER_CHAR:	")) {
				//CONFIG_PROMPT_FILLER_CHAR = buf[31];
				ParseCharOrCodePoint(buf + 31, CONFIG_PROMPT_FILLER_CHAR);
#ifdef DEBUG
				printf("CONFIG:%s : %s -> '%s'\n", buf, buf + 31, CONFIG_PROMPT_FILLER_CHAR);
#endif
			} else if (Compare(buf, "TEMP_OVERRIDE_OLD_GIT_VERSION")) {
				I_HAVE_ANCIENT_GIT = true;
			} else {
				fprintf(stderr, "Warning: unknown entry in config file: >%s<\n", buf);
				UnknownConfig = true;
			}
		}
	}
	fclose(fp);
	free(buf);
	if (!CONFIG_DISABLE_LOCS_CHECKING) {
		for (int i = 0; i < numLOCS; i++) {
			char* temp = FixImplicitProtocol(LOCS[i]);
			if (!Compare(temp, LOCS[i])) {
				fprintf(stderr, "WARNING: one of your ORIGIN_ALIAS definitions (%s) has an implicit protocol, please correct it (likely should be %s).\n\tIf you are sure it works as you expect, you can disable this warning in the config file (DISABLE_ORIGIN_ALIAS_ERROR_CHECKING:	true)\n", LOCS[i], temp);
			}
			free(temp);
		}
	}
	if (UnknownConfig) {
		fprintf(stderr, "WARNING: You have unknown entires in your config file (%s/config.cfg).\n\tPlease check the template at %s/DEFAULTCONFIG.cfg for a list of all understood options and correct your own config file\n", getenv("ST_CFG"), getenv("ST_SRC"));
	}
	regfree(&ConfigRegex);
}

void Cleanup() {
	for (int i = 0; i < MaxLocations; i++) {
		if (LOCS[i] != NULL) {
			free(LOCS[i]);
			LOCS[i] = NULL;
		};
		if (NAMES[i] != NULL) {
			free(NAMES[i]);
			NAMES[i] = NULL;
		};
		if (GitHubs[i] != NULL) {
			free(GitHubs[i]);
			GitHubs[i] = NULL;
		};
	}
}
