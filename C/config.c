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
#include <stdlib.h> // for free, malloc, atoi, secure_getenv, exit, strtol
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

#define MAX_FMT_LEN 32

bool CONFIG_LOWPROMPT_INDICATE_VENV = true;
char CONFIG_LOWPROMPT_VENV_FORMAT[MAX_FMT_LEN];
bool CONFIG_LOWPROMPT_PATH_LIMIT = true;
int CONFIG_LOWPROMPT_PATH_MAXLEN = -3;
char CONFIG_LOWPROMPT_PATH_FORMAT[MAX_FMT_LEN];
bool CONFIG_LOWPROMPT_RETCODE = true;
bool CONFIG_LOWPROMPT_RETCODE_DECODE = true;
char CONFIG_LOWPROMPT_RETCODE_OK_FORMAT[MAX_FMT_LEN];
char CONFIG_LOWPROMPT_RETCODE_ERROR_FORMAT[MAX_FMT_LEN];
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
bool CONFIG_PROMPT_JOB_DETAILS = true;
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

static bool ParseCfgBool(const char* inputbuffer, const char* pattern, bool* resultSetting) {
	if (StartsWith(inputbuffer, pattern)) {
		size_t len = strlen(pattern);
		*resultSetting = Compare(inputbuffer + len, "true");
#ifdef DEBUG
		printf("CONFIG:%s : %s -> %i\n", inputbuffer, inputbuffer + len, *resultSetting);
#endif
		return true;
	}
	return false;
}

static bool ParseCfgChar(const char* inputbuffer, const char* pattern, char* resultSetting) {
	if (StartsWith(inputbuffer, pattern)) {
		size_t len = strlen(pattern);
		ParseCharOrCodePoint(inputbuffer + len, resultSetting);
#ifdef DEBUG
		printf("CONFIG:%s : %s -> '%s'\n", inputbuffer, inputbuffer + len, resultSetting);
#endif
		return true;
	}
	return false;
}

static bool ParseCfgInt_(const char* inputbuffer, const char* pattern, int* resultSetting) {
	if (StartsWith(inputbuffer, pattern)) {
		size_t len = strlen(pattern);
		*resultSetting = atoi(inputbuffer + len);
#ifdef DEBUG
		printf("CONFIG:%s : %s -> %i\n", inputbuffer, inputbuffer + len, *resultSetting);
#endif
		return true;
	}
	return false;
}

/**
 * ANSI Escape Code parsing
 */
static bool ParseCfgANSI(char* inputbuffer, const char* pattern, char* resultSetting) {
	if (StartsWith(inputbuffer, pattern)) {
		size_t len = strlen(pattern);
		TerminateStrOn(inputbuffer + len + 1, "\"");
		CopyStringNumCharConfig(resultSetting, inputbuffer + len + 1, MAX_FMT_LEN - 1, true);
#ifdef DEBUG
		printf("CONFIG:%s : %s -> %sDEMO Formatting\e[0m\n", inputbuffer, inputbuffer + len + 1, resultSetting);
#endif
		return true;
	}
	return false;
}

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

	CopyStringNumChar(CONFIG_LOWPROMPT_VENV_FORMAT, "\e[35m", MAX_FMT_LEN - 1);
	CopyStringNumChar(CONFIG_LOWPROMPT_PATH_FORMAT, "\e[36m\e[1m", MAX_FMT_LEN - 1);
	CopyStringNumChar(CONFIG_LOWPROMPT_RETCODE_OK_FORMAT, "\e[32m\e[1m", MAX_FMT_LEN - 1);
	CopyStringNumChar(CONFIG_LOWPROMPT_RETCODE_ERROR_FORMAT, "\e[31m\e[1m", MAX_FMT_LEN - 1);

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
	const char* pointerIntoEnv = secure_getenv("ST_CFG");
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
			const char* defaultConfigFileDir = secure_getenv("ST_SRC");
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
		if (buf[0] == '#' || buf[0] == 0x00) {
			continue;
		} else {
			if (TerminateThenTrimStrOn(buf, DEFAULT_TERMINATORS "#", " \t") == 0) {
				continue;
			}

			{ //repo origin alias handling
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
					printf("CONFIG:origin>%s|%s|%i<\n", NAMES[numLOCS], LOCS[numLOCS], GROUPS[numLOCS]);
#endif
					numLOCS++;
					continue;
				}
			}

			if (StartsWith(buf, "GITHUB_HOST:	")) {
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
					printf("CONFIG:host>%s<\n", GitHubs[numGitHubs]);
#endif
					numGitHubs++;
					continue;
				}
				//the +13 is the offset to just after "GITHUB_HOST:	"
			}

			if (StartsWith(buf, "GIT_EXCLUSION:	")) {
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
					continue;
				}
			}

			if (Compare(buf, "TEMP_OVERRIDE_OLD_GIT_VERSION")) {
				I_HAVE_ANCIENT_GIT = true;
				continue;
			}

			if (ParseCfgBool(buf, "DISABLE_ORIGIN_ALIAS_ERROR_CHECKING:	", &CONFIG_DISABLE_LOCS_CHECKING)) continue;

			if (ParseCfgBool(buf, "SHELLTOOLS.GIT.AUTO_RESTORE_EXCLUSION:	", &CONFIG_GIT_AUTO_RESTORE_EXCLUSION)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LOWPROMPT.VENV.ENABLE:	", &CONFIG_LOWPROMPT_INDICATE_VENV)) continue;
			if (ParseCfgANSI(buf, "SHELLTOOLS.LOWPROMPT.VENV.FORMATTING:	", &(CONFIG_LOWPROMPT_VENV_FORMAT[0]))) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LOWPROMPT.PATH.LIMIT_DISPLAY_LENGTH.ENABLE:	", &CONFIG_LOWPROMPT_PATH_LIMIT)) continue;
			if (ParseCfgInt_(buf, "SHELLTOOLS.LOWPROMPT.PATH.LIMIT_DISPLAY_LENGTH.TARGET:	", &CONFIG_LOWPROMPT_PATH_MAXLEN)) continue;
			if (ParseCfgANSI(buf, "SHELLTOOLS.LOWPROMPT.PATH.FORMATTING:	", &(CONFIG_LOWPROMPT_PATH_FORMAT[0]))) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LOWPROMPT.RETURNCODE.ENABLE:	", &CONFIG_LOWPROMPT_RETCODE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LOWPROMPT.RETURNCODE.DECODE.ENABLE:	", &CONFIG_LOWPROMPT_RETCODE_DECODE)) continue;
			if (ParseCfgANSI(buf, "SHELLTOOLS.LOWPROMPT.RETURNCODE.OK.FORMATTING:	", &(CONFIG_LOWPROMPT_RETCODE_OK_FORMAT[0]))) continue;
			if (ParseCfgANSI(buf, "SHELLTOOLS.LOWPROMPT.RETURNCODE.ERROR.FORMATTING:	", &(CONFIG_LOWPROMPT_RETCODE_ERROR_FORMAT[0]))) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LOWPROMPT.COMMAND_TIMER.ENABLE:	", &CONFIG_LOWPROMPT_TIMER)) continue;
			if (ParseCfgChar(buf, "SHELLTOOLS.LOWPROMPT.START_CHAR:	", CONFIG_LOWPROMPT_START_CHAR)) continue;
			if (ParseCfgChar(buf, "SHELLTOOLS.LOWPROMPT.END_CHAR:	", CONFIG_LOWPROMPT_END_CHAR)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.SSHINFO.ENABLE:	", &CONFIG_PROMPT_SSH)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.TERMINALDEVICE.ENABLE:	", &CONFIG_PROMPT_TERMINAL_DEVICE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.TIME.ENABLE:	", &CONFIG_PROMPT_TIME)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.TIMEZONE.ENABLE:	", &CONFIG_PROMPT_TIMEZONE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.DATE.ENABLE:	", &CONFIG_PROMPT_DATE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.CALENDARWEEK.ENABLE:	", &CONFIG_PROMPT_CALENDARWEEK)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.PROXYSTATUS.ENABLE:	", &CONFIG_PROMPT_PROXY)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.NETWORK.ENABLE:	", &CONFIG_PROMPT_NETWORK)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.GIT.REMOTE.ENABLE:	", &CONFIG_PROMPT_GIT_REMOTE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.GIT.BRANCH.OVERVIEW.ENABLE:	", &CONFIG_PROMPT_GIT_BRANCHSTATUS)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.GIT.BRANCHSTATUS.ENABLE:	", &CONFIG_PROMPT_GIT_BRANCHSTATUS)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.GIT.BRANCH.ENABLE:	", &CONFIG_PROMPT_GIT_BRANCHNAME)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.GIT.REPONAME.ENABLE:	", &CONFIG_PROMPT_GIT_REPONAME)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.GIT.LOCALCHANGES.ENABLE:	", &CONFIG_PROMPT_GIT_GITSTATUS)) continue;
			if (ParseCfgInt_(buf, "SHELLTOOLS.PROMPT.GIT.MAXBRANCHES:	", &CONFIG_PROMPT_GIT_BRANCHLIMIT)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.GIT.WARN_BRANCH_LIMIT:	", &CONFIG_PROMPT_GIT_WARN_BRANCHLIMIT)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.USER.ENABLE:	", &CONFIG_PROMPT_USER)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.HOST.ENABLE:	", &CONFIG_PROMPT_HOST)) continue;
			if (ParseCfgChar(buf, "SHELLTOOLS.PROMPT.FILLER_CHAR:	", CONFIG_PROMPT_FILLER_CHAR)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.BACKGROUNDJOBS.ENABLE:	", &CONFIG_PROMPT_JOBS)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.BACKGROUNDJOBS.DETAILS.ENABLE:	", &CONFIG_PROMPT_JOB_DETAILS)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.POWER.ENABLE:	", &CONFIG_PROMPT_POWER)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.ENABLE:	", &CONFIG_PROMPT_OVERALL_ENABLE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.GIT.ENABLE:	", &CONFIG_PROMPT_GIT)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.NETWORK.INTERFACES.DEFAULT.ENABLE:	", &CONFIG_PROMPT_NET_IFACE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.NETWORK.INTERFACES.NONDEFAULT.ENABLE:	", &CONFIG_PROMPT_NET_ADDITIONAL)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.NETWORK.ROUTINGINFO.ENABLE:	", &CONFIG_PROMPT_NET_ROUTE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.NETWORK.LINKSPEED.ENABLE:	", &CONFIG_PROMPT_NET_LINKSPEED)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.GIT.REPOTYPE.ENABLE:	", &CONFIG_PROMPT_GIT_REPOTYPE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.GIT.REPOTYPE.PARENT_REPO.ENABLE:	", &CONFIG_PROMPT_GIT_REPOTYPE_PARENT)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.PROMPT.GIT.COMMIT_OVERVIEW.ENABLE:	", &CONFIG_PROMPT_GIT_COMMITS)) continue;

			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.WARN_BRANCH_LIMIT:	", &CONFIG_LSGIT_WARN_BRANCHLIMIT)) continue;

			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.QUICK.REPOTYPE.ENABLE:	", &CONFIG_LSGIT_QUICK_REPOTYPE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.QUICK.REPOTYPE.PARENT_REPO.ENABLE:	", &CONFIG_LSGIT_QUICK_REPOTYPE_PARENT)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.QUICK.REPONAME.ENABLE:	", &CONFIG_LSGIT_QUICK_REPONAME)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.QUICK.BRANCH.ENABLE:	", &CONFIG_LSGIT_QUICK_BRANCHNAME)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.QUICK.BRANCH.OVERVIEW.ENABLE:	", &CONFIG_LSGIT_QUICK_REMOTE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.QUICK.REMOTE.ENABLE:	", &CONFIG_LSGIT_QUICK_REMOTE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.QUICK.COMMIT_OVERVIEW.ENABLE:	", &CONFIG_LSGIT_QUICK_COMMITS)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.QUICK.LOCALCHANGES.ENABLE:	", &CONFIG_LSGIT_QUICK_GITSTATUS)) continue;
			if (ParseCfgInt_(buf, "SHELLTOOLS.LSGIT.QUICK.MAXBRANCHES:	", &CONFIG_LSGIT_QUICK_BRANCHLIMIT)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.QUICK.BRANCHSTATUS.ENABLE:	", &CONFIG_LSGIT_QUICK_BRANCHSTATUS)) continue;

			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.THOROUGH.REPOTYPE.ENABLE:	", &CONFIG_LSGIT_THOROUGH_REPOTYPE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.THOROUGH.REPOTYPE.PARENT_REPO.ENABLE:	", &CONFIG_LSGIT_THOROUGH_REPOTYPE_PARENT)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.THOROUGH.REPONAME.ENABLE:	", &CONFIG_LSGIT_THOROUGH_REPONAME)) continue;
			if (ParseCfgInt_(buf, "SHELLTOOLS.LSGIT.THOROUGH.MAXBRANCHES:	", &CONFIG_LSGIT_THOROUGH_BRANCHLIMIT)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.THOROUGH.BRANCH.ENABLE:	", &CONFIG_LSGIT_THOROUGH_BRANCHNAME)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.THOROUGH.BRANCH.OVERVIEW.ENABLE:	", &CONFIG_LSGIT_THOROUGH_BRANCHINFO)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.THOROUGH.REMOTE.ENABLE:	", &CONFIG_LSGIT_THOROUGH_REMOTE)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.THOROUGH.COMMIT_OVERVIEW.ENABLE:	", &CONFIG_LSGIT_THOROUGH_COMMITS)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.THOROUGH.LOCALCHANGES.ENABLE:	", &CONFIG_LSGIT_THOROUGH_GITSTATUS)) continue;
			if (ParseCfgBool(buf, "SHELLTOOLS.LSGIT.THOROUGH.BRANCHSTATUS.ENABLE:	", &CONFIG_LSGIT_THOROUGH_BRANCHSTATUS)) continue;

			//if I reach this point I didn't hit 'continue;' in any case handled above -> print warning
			fprintf(stderr, "Warning: unknown entry in config file: >%s<\n", buf);
			UnknownConfig = true;
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
		fprintf(stderr, "WARNING: You have unknown entires in your config file (%s/config.cfg).\n\tPlease check the template at %s/DEFAULTCONFIG.cfg for a list of all understood options and correct your own config file\n", secure_getenv("ST_CFG"), secure_getenv("ST_SRC"));
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
