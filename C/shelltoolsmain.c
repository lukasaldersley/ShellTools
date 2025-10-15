#/*
TargetDir="$ST_CFG"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename "$0" .c).elf"
printf "compiling %s into %s/%s" "$0" "$TargetDir" "$TargetName"
SourceDirectory="$(realpath "$(dirname "$0")")"
gcc -O3 -std=c2x -Wall "$SourceDirectory/commons.c" "$SourceDirectory/inetfunc.c" "$SourceDirectory/config.c" "$SourceDirectory/gitfunc.c" "$0" -o "$TargetDir/$TargetName" "$@"
#I WANT to be able to do things like ./shelltoolsmain.c -DPROFILING to add the compiler flag profiling but ALSO stuff like ./shelltoolsmain "-DDEBUG -DPROFILING" to add both profiling and debug
printf " -> \e[32mDONE\e[0m(%s)\n" $?
exit
*/

#include "commons.h" // for ABORT_NO_MEMORY, strlen_visible, Compare, TerminateStrOn, DEFAULT_TERMINATORS, COLOUR_CLEAR, ExecuteProcess_alloc, AbbreviatePathAuto, StartsWith, determinePossibleCombinations, COLOUR_GREYOUT
#include "config.h" // for CONFIG_PROMPT_FILLER_CHAR, CONFIG_GIT_BRANCHNAME, CONFIG_PROMPT_HOST, CONFIG_GIT_BRANCHSTATUS, CONFIG_GIT_REPONAME, CONFIG_GIT_REPOTYPE, CONFIG_LOWPROMPT_PATH_MAXLEN, CONFIG_LOWPROMPT_RETCODE, CONFIG_PROMPT_NETWORK, CONFIG_PROMPT_USER, CONFIG_GIT_BRANCH_OVERVIEW, CONFIG_GIT_COMMIT_OVERVIEW, CONFIG_GIT_LOCALCHANGES, CONFIG_GIT_REMOTE, CONFIG_GIT_REPOTYPE_PARENT, CONFIG_LOWPROMPT_TIMER, CONFIG_PROMPT_GIT, CONFIG_PROMPT_TERMINAL_DEVICE, Cleanup, DoSetup, CONFIG_GIT_AUTO_RESTO...
#include "gitfunc.h" // for AllocRepoInfo, AllocUnsetStringsToEmpty, ConstructCommitStatusString, ConstructGitBranchInfoString, ConstructGitStatusString, DeallocRepoInfoStrings, TestPathForRepoAndParseIfExists, gitfunc_deinit, gitfunc_init, COLOUR_GIT_BARE, COLOUR_GIT_BRANCH, COLOUR_GIT_INDICATOR, COLOUR_GIT_NAME, COLOUR_GIT_ORIGIN, COLOUR_GIT_PARENT, RepoInfo
#include "inetfunc.h" // for GetBaseIPString, IpTransportStruct

#include <assert.h> // for assert
#include <dirent.h> // for dirent, closedir, opendir, readdir, DIR
#include <getopt.h> // for option, getopt_long, no_argument
#include <regex.h> // for regcomp, regerror, regexec, regfree, REG_EXTENDED, REG_NEWLINE, regex_t, regmatch_t
#include <signal.h> // for SIGABRT, SIGALRM, SIGBUS, SIGCHLD, SIGCONT, SIGFPE, SIGHUP, SIGILL, SIGINT, SIGKILL, SIGPIPE, SIGPOLL, SIGPROF, SIGPWR, SIGQUIT, SIGSEGV, SIGSTKFLT, SIGSTOP, SIGSYS, SIGTERM, SIGTRAP, SIGTSTP, SIGTTIN, SIGTTOU, SIGURG, SIGUSR1, SIGUSR2, SIGVTALRM, SIGWINCH, SIGXCPU, SIGXFSZ
#include <stdbool.h> // for bool, false, true
#include <stdint.h> // for uint8_t, UINT8_MAX, uint32_t
#include <stdio.h> // for printf, NULL, snprintf, asprintf, fclose, fflush, fgets, fopen, stdout, fprintf, putc, stderr, fileno, perror, remove, FILE, stdin
#include <stdlib.h> // for free, malloc, secure_getenv, exit, atoi, strtol
#include <string.h> // for strncpy
#include <sys/ioctl.h> // for ioctl, winsize, TIOCGWINSZ
#include <time.h> // for strftime, localtime, time, time_t
#include <unistd.h> // for NULL, optarg, access, optind, F_OK

#define COLOUR_TERMINAL_DEVICE "\e[38;5;242m"
#define COLOUR_SHLVL		   "\e[0m"
#define COLOUR_USER			   "\e[38;5;010m"
#define COLOUR_USER_AT_HOST	   "\e[38;5;007m"
#define COLOUR_HOST			   "\e[38;5;033m"

typedef enum {
	IP_MODE_NONE,
	IP_MODE_LEGACY,
	IP_MODE_STANDALONE
} IP_MODE;

#ifdef PROFILING
bool ALLOW_PROFILING_TESTPATH = false;
#define PROFILE_MAIN_ENTRY			0
#define PROFILE_MAIN_ARGS			1
#define PROFILE_CONFIG_READ			2
#define PROFILE_MAIN_COMMON_END		3
#define PROFILE_MAIN_PROMPT_ARGS_IP 4
//TestpathForRepoAndParseIfExists, requires ALLOW_PROFILING_TESTPATH to be set
#define PROFILE_MAIN_PROMPT_PATHTEST_BASE	  PROFILE_MAIN_PROMPT_ARGS_IP
#define PROFILE_TESTPATH_BASIC_CHECKS		  5
#define PROFILE_TESTPATH_WorktreeChecks		  6
#define PROFILE_BRANCHING_HAVE_BRANCHES		  7
#define PROFILE_TESTPATH_BranchChecked		  8
#define PROFILE_TESTPATH_ExtendedGitStatus	  9
#define PROFILE_TESTPATH_remote_and_reponame  10
#define PROFILE_MAIN_PROMPT_PathGitTested	  11
#define PROFILE_MAIN_PROMPT_GitBranchObtained 12
#define PROFILE_MAIN_PROMPT_CommitOverview	  13
#define PROFILE_MAIN_PROMPT_GitStatus		  14
#define PROFILE_MAIN_PROMPT_PREP_COMPLETE	  15
#define PROFILE_MAIN_PROMPT_PRINTING_DONE	  16
#define PROFILE_MAIN_SETSHOW_SETUP			  17
#define PROFILE_MAIN_SETSHOW_COMPLETE		  18
#define PROFILE_MAIN_END					  19
#define PROFILE_COUNT						  20
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
	"exit",
};

static double calcProfilingTime(uint8_t startIndex, uint8_t stopIndex) {
	uint64_t tstart = profiling_timestamp[startIndex].tv_sec * 1000000ULL + (profiling_timestamp[startIndex].tv_nsec / 1000ULL);
	uint64_t tstop = profiling_timestamp[stopIndex].tv_sec * 1000000ULL + (profiling_timestamp[stopIndex].tv_nsec / 1000ULL);
	return (double)(tstop - tstart) / 1000.0;
}
#endif

#define POWER_CHARGE		"\e[38;5;157m▲ "
#define POWER_DISCHARGE		"\e[38;5;217m▽ "
#define POWER_FULL			"\e[38;5;157m≻ "
#define POWER_NOCHARGE_HIGH "\e[38;5;172m◊ "
#define POWER_NOCHARGE_LOW	"\e[38;5;009m◊ "
#define POWER_UNKNOWN		"\e[38;5;9m!⌧? "
#define CHARGE_AC			"≈"
#define CHARGE_USB			"≛"
#define CHARGE_BOTH			"⩰"
#define CHARGER_CONNECTED	"↯ "

typedef enum {
	CHARGING,
	DISCHARGING,
	FULL,
	NOT_CHARGING,
	UNKNOWN
} chargestate;

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
	if (buf == NULL) ABORT_NO_MEMORY;
	buf[buf_max_len] = 0x00;

	bool IsBat = false;
	bool IsUSB = false;
	bool IsMains = false;
	chargestate state = UNKNOWN;
	uint8_t len = 0;

	if (asprintf(&path, "%s/%s/type", directory, dir) == -1) ABORT_NO_MEMORY;
	fp = fopen(path, "r");
	if (fp != NULL) {
		if (fgets(buf, buf_max_len - 1, fp) != NULL) {
			TerminateStrOn(buf, DEFAULT_TERMINATORS);
			if (Compare("Battery", buf)) {
				IsBat = true;
			} else if (Compare("USB", buf)) {
				IsUSB = true;
			} else if (Compare("Mains", buf)) {
				IsMains = true;
			} else {
				printf("WARNING: Unknown type %s for %s/%s. Please report to ShellTools developer\n", buf, directory, dir);
			}
		}
		fclose(fp);
		fp = NULL;
	}
	if (IsBat) {
		long percent = 0;
		free(path);
		if (asprintf(&path, "%s/%s/status", directory, dir) == -1) ABORT_NO_MEMORY;
		fp = fopen(path, "r");
		if (fp != NULL) {
			if (fgets(buf, buf_max_len - 1, fp) != NULL) {
				TerminateStrOn(buf, DEFAULT_TERMINATORS);
				if (Compare(buf, "Charging")) {
					state = CHARGING;
				} else if (Compare(buf, "Discharging")) {
					state = DISCHARGING;
				} else if (Compare(buf, "Full")) {
					state = FULL;
				} else if (Compare(buf, "Not charging")) {
					state = NOT_CHARGING;
				} else if (Compare(buf, "Unknown")) {
					state = UNKNOWN;
				} else {
					printf("unknown status '%s' for %s/%s. Please report to ShellTools delevoper\n", buf, directory, dir);
				}
			}
			fclose(fp);
			fp = NULL;
		}
		free(path);
		if (asprintf(&path, "%s/%s/capacity", directory, dir) == -1) ABORT_NO_MEMORY;
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
		} else {
			switch (state) {
				case CHARGING: {
					len += snprintf(obuf, avlen - len, POWER_CHARGE "%li%%\e[0m ", percent);
					break;
				}
				case DISCHARGING: {
					len += snprintf(obuf, avlen - len, POWER_DISCHARGE "%li%%\e[0m ", percent);
					break;
				}
				case FULL: {
					len += snprintf(obuf, avlen - len, POWER_FULL "%li%%\e[0m ", percent);
					break;
				}
				case NOT_CHARGING: {
					if (percent >= 95) {
						len += snprintf(obuf, avlen - len, POWER_NOCHARGE_HIGH "%li%%\e[0m ", percent);
					} else {
						len += snprintf(obuf, avlen - len, POWER_NOCHARGE_LOW "%li%%\e[0m ", percent);
					}
					break;
				}
				case UNKNOWN: {
					len += snprintf(obuf, avlen - len, POWER_UNKNOWN "%li%%\e[0m ", percent);
					break;
				}
			}
		}
	} else {
		free(path);
		if (asprintf(&path, "%s/%s/online", directory, dir) == -1) ABORT_NO_MEMORY;
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
	//TODO check the /sys/class/power_supply/whatever/scope file. "Device" was the battery of a connected MX Master 3S mouse, the system itself is a PC where power monitoring isn't needed. if you can query a file at all you have power, else you'd be staring at a dead black screen, but providing peripheral integration would be f**ing awesome
	//If I could replicate whatever KDE reports in it's bluetooth menu (stuff like the mentioned MX Master, but also the various headphones I have...)
	//parsing "bluetoothctl info" would do the trick
#define POWER_CHARS_PER_BAT 64
#define POWER_NUM_BAT		2
#define POWER_CHARS_EXTPWR	8
	assert(POWER_CHARS_EXTPWR + POWER_CHARS_PER_BAT * POWER_NUM_BAT < UINT8_MAX);
	uint8_t powerBatMaxLen = POWER_CHARS_PER_BAT * POWER_NUM_BAT;
	uint8_t powerMaxLen = powerBatMaxLen + POWER_CHARS_EXTPWR;
	char* powerString = malloc(sizeof(char) * powerMaxLen + 1);
	if (powerString == NULL) ABORT_NO_MEMORY;
	powerString[powerMaxLen] = 0x00;
	powerString[0] = 0x00;
	uint8_t currentLen = 0;

	PowerBitField field;
	field.IsWSL = secure_getenv("WSL_VERSION") != NULL;
	field.IsMains = false;
	field.IsUSB = false;
	DIR* directoryPointer;
	const char* path = "/sys/class/power_supply";
	directoryPointer = opendir(path);
	if (directoryPointer != NULL) {
		//On success, readdir() returns a pointer to a dirent structure.  (This structure may be statically allocated; do not attempt to free(3) it.)
		const struct dirent* direntptr;
		while ((direntptr = readdir(directoryPointer))) {
			if (/*direntptr->d_type == DT_LNK && */ !Compare(direntptr->d_name, ".") && !Compare(direntptr->d_name, "..")) {
				currentLen += ParsePowerSupplyEntry(path, direntptr->d_name, &field, powerString + currentLen, powerBatMaxLen - currentLen);
			}
		}
		closedir(directoryPointer);
	} else {
		fprintf(stderr, "failed on directory: %s\n", path);
		perror("Couldn't open the directory");
	}
	if (field.IsMains || field.IsUSB) {
		//WSL only ever reports as usb -> only report details if not WSL
		if (!field.IsWSL) {

			if (field.IsMains && field.IsUSB) {
				//both
				currentLen += snprintf(powerString + currentLen, powerMaxLen - currentLen, "%s", CHARGE_BOTH);
			} else if (field.IsMains) {
				//only mains
				currentLen += snprintf(powerString + currentLen, powerMaxLen - currentLen, "%s", CHARGE_AC);
			} else {
				//only USB
				currentLen += snprintf(powerString + currentLen, powerMaxLen - currentLen, "%s", CHARGE_USB);
			}
		}
		snprintf(powerString + currentLen, powerMaxLen - currentLen, CHARGER_CONNECTED);
	}
	//printf("PowerString: <%s> (%i/%i)", powerString, strlen(powerString), strlen_visible(powerString));
	//printf("\n");
	return powerString;
}

int main(int argc, char** argv) {
	printf("...\r");
	fflush(stdout);
#ifdef PROFILING
	for (int i = 0; i < PROFILE_COUNT; i++) {
		profiling_timestamp[i].tv_nsec = 0;
		profiling_timestamp[i].tv_sec = 0;
	}
	timespec_get(&(profiling_timestamp[PROFILE_MAIN_ENTRY]), TIME_UTC);
#endif
	int main_retcode = 0;

	gitfunc_init();

	//stuff to obtain the time/date/calendarweek for prompt
	time_t tm;
	tm = time(NULL);
	const struct tm* localtm = localtime(&tm);

	bool IsPrompt = 0, IsLowPrompt = 0;

	int PromptRetCode = 0;
	//the whole song and dance with windowProps, ioctl, magic constants etc is to obtain the current terminal width in a reliable and portable fashion
	struct winsize windowProps;
	//note: stdout did NOT work for LOWPROMPT and returns bogus data (I have seen 0 or mid 4 digits), stdin seems to work reliably
	ioctl(fileno(stdin), TIOCGWINSZ, &windowProps);
	int Arg_TotalPromptWidth = windowProps.ws_col;
#ifdef DEBUG
	printf("ScreenWidth: %i\n", Arg_TotalPromptWidth);
#endif

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

	int getopt_currentChar; //the char for getop switch
	int option_index = 0;

	const static struct option long_options[] = {
		{"lowprompt", no_argument, 0, '4'},
		{"prompt", no_argument, 0, '0'},
		{0, 0, 0, 0},
	};

	while (1) {

		getopt_currentChar = getopt_long(argc, argv, "i:j:p:r:t:", long_options, &option_index);
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
			case '0': {
				if (IsLowPrompt) {
					printf("prompt is mutex with lowprompt\n");
					break;
				}
				IsPrompt = 1;
				break;
			}
			case '4': {
				if (IsPrompt) {
					printf("lowprompt is mutex with prompt\n");
					break;
				}
				IsLowPrompt = 1;
				break;
			}
			case 'i': {
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
			case 'j': {
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_BackgroundJobs = optarg;
				Arg_BackgroundJobs_len = strlen_visible(Arg_BackgroundJobs);
				break;
			}
			case 'p': {
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_ProxyInfo = optarg;
				Arg_ProxyInfo_len = strlen_visible(Arg_ProxyInfo);
				break;
			}
			case 'r': {
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				PromptRetCode = atoi(optarg);
				break;
			}
			case 't': {
				TerminateStrOn(optarg, DEFAULT_TERMINATORS);
				Arg_CmdTime = optarg;
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

	if (!(IsPrompt || IsLowPrompt)) {
		printf("you must specify EITHER --prompt or --lowprompt\n");
		exit(1);
	}

	if (!(optind < argc)) {
		printf("You must supply one non-option parameter (the path)");
	}
	const char* path = argv[optind];

#ifdef PROFILING
	timespec_get(&(profiling_timestamp[PROFILE_MAIN_ARGS]), TIME_UTC);
#endif

	DoSetup(); //this reads the config file -> as of hereI can expect to have current options

#ifdef PROFILING
	timespec_get(&(profiling_timestamp[PROFILE_CONFIG_READ]), TIME_UTC);
#endif

	if (IsPrompt) {
		if (CONFIG_PROMPT_POWER) {
			Arg_PowerState = GetSystemPowerState();
			Arg_PowerState_len = strlen_visible(Arg_PowerState);
		} else {
			Arg_PowerState = malloc(sizeof(char));
			if (Arg_PowerState == NULL) ABORT_NO_MEMORY;
			Arg_PowerState[0] = 0x00;
			Arg_PowerState_len = 0;
		}
		if (CONFIG_PROMPT_TERMINAL_DEVICE) {
			Arg_TerminalDevice = ExecuteProcess_alloc("/usr/bin/tty");
			TerminateStrOn(Arg_TerminalDevice, DEFAULT_TERMINATORS);
			Arg_TerminalDevice_len = strlen_visible(Arg_TerminalDevice) + 1; //NOTE, the +1 is for the : added at print time
		} else {
			Arg_TerminalDevice = malloc(sizeof(char));
			if (Arg_TerminalDevice == NULL) ABORT_NO_MEMORY;
			Arg_TerminalDevice[0] = 0x00;
			Arg_TerminalDevice_len = 0;
		}
		if (!CONFIG_PROMPT_PROXY && Arg_ProxyInfo != NULL) {
			Arg_ProxyInfo[0] = 0x00;
			Arg_ProxyInfo_len = 0;
		}
		if (!CONFIG_PROMPT_JOBS && Arg_BackgroundJobs != NULL) {
			Arg_BackgroundJobs[0] = 0x00;
			Arg_BackgroundJobs_len = 0;
		}

		if (CONFIG_PROMPT_SSH) {
			char* ssh = secure_getenv("SSH_CLIENT");
			if (ssh != NULL) {
				//echo "$SSH_CONNECTION" | sed -nE 's~^([-0-9a-zA-Z_\.:]+) ([0-9]+) ([-0-9a-zA-Z_\.:]+) ([0-9]+)$~<SSH: \1:\2 -> \3:\4> ~p'
				uint8_t SSHRegexGroupCount = 6;
				regmatch_t SSHRegexGroups[SSHRegexGroupCount];
				regex_t SSHRegex;
				const char* SSHRegexString = "^(([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)|([0-9a-fA-F:]+)) ([0-9]+) ([0-9]+)$";
#define SSHRegexIP		   1
#define SSHRegexIPv4	   2
#define SSHRegexIPv6	   3
#define SSHRegexRemotePort 4
#define SSHRegexMyPort	   5
				int SSHRegexReturnCode;
				SSHRegexReturnCode = regcomp(&SSHRegex, SSHRegexString, REG_EXTENDED | REG_NEWLINE);
				if (SSHRegexReturnCode) {
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
						Arg_SSHInfo = malloc(sizeof(char) * tlen); //"<SSH: [x]:x -> port x>", regex group 0 contains all three x plus 2 spaces -> if I add 18 I should be fine <SSH: [::1]:55450 -> port 22>
						if (Arg_SSHInfo == NULL) ABORT_NO_MEMORY;
						Arg_SSHInfo_len = 0;
						Arg_SSHInfo[0] = 0x00;
						Arg_SSHInfo_len += snprintf(Arg_SSHInfo, tlen - (Arg_SSHInfo_len + 1), "<SSH: ");

						int ilen = SSHRegexGroups[SSHRegexIPv4].rm_eo - SSHRegexGroups[SSHRegexIPv4].rm_so;
						if (ilen > 0) {
							strncpy(Arg_SSHInfo + Arg_SSHInfo_len, ssh + SSHRegexGroups[SSHRegexIPv4].rm_so, ilen);
							Arg_SSHInfo_len += ilen;
						} else {
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
			if (Arg_SSHInfo == NULL) ABORT_NO_MEMORY;
			Arg_SSHInfo[0] = 0x00;
			Arg_SSHInfo_len = 0;
		}
		fflush(stdout);

		const char* lvl = secure_getenv("SHLVL");
		if (!Compare("1", lvl)) {
			if (asprintf(&Arg_SHLVL, " [%s]", lvl) == -1) ABORT_NO_MEMORY;
			Arg_SHLVL_len = strlen_visible(Arg_SHLVL);
		} else {
			Arg_SHLVL = malloc(sizeof(char));
			if (Arg_SHLVL == NULL) ABORT_NO_MEMORY;
			Arg_SHLVL[0] = 0x00;
			Arg_SHLVL_len = 0;
		}

		Time = malloc(sizeof(char) * 16);
		if (Time == NULL) ABORT_NO_MEMORY;
		if (CONFIG_PROMPT_TIME) {
			Time_len = strftime(Time, 16, "%T ", localtm);
		} else {
			Time_len = 0;
			Time[0] = 0x00;
		}

		int TimezoneStringMaxSize = 17; //" UTC+dddd (ABCD)",->16 chars+nullbyte=17 both snprintf and strftime include the nullbyte in their count
		TimeZone = malloc(sizeof(char) * (TimezoneStringMaxSize));
		if (TimeZone == NULL) ABORT_NO_MEMORY;
		if (CONFIG_PROMPT_TIMEZONE) {
			if (localtm->tm_gmtoff == 0) {
				TimeZone_len = snprintf(TimeZone, TimezoneStringMaxSize, "UTC "); //snprintf's max size param includes nullbyte
			} else {
				TimeZone_len = strftime(TimeZone, TimezoneStringMaxSize, "UTC%z (%Z) ", localtm);
			}
		} else {
			TimeZone_len = 0;
			TimeZone[0] = 0x00;
		}

		DateInfo = malloc(sizeof(char) * 16);
		if (DateInfo == NULL) ABORT_NO_MEMORY;
		if (CONFIG_PROMPT_DATE) {
			DateInfo_len = strftime(DateInfo, 16, "%a %d.%m.%Y ", localtm);
		} else {
			DateInfo_len = 0;
			DateInfo[0] = 0x00;
		}

		CalendarWeek = malloc(sizeof(char) * 8);
		if (CalendarWeek == NULL) ABORT_NO_MEMORY;
		if (CONFIG_PROMPT_CALENDARWEEK) {
			CalendarWeek_len = strftime(CalendarWeek, 8, "KW%V ", localtm);
		} else {
			CalendarWeek_len = 0;
			CalendarWeek[0] = 0x00;
		}
	}

	//this whole block may seem nonsensical, but these values are used in various git functions defined in a library used by several different applications
	//this then sets the general values to what PROMPT needs (the XOR of this is in repotree.c)
	CONFIG_GIT_MAXBRANCHES = CONFIG_PROMPT_GIT_BRANCHLIMIT;
	CONFIG_GIT_WARN_BRANCH_LIMIT = CONFIG_PROMPT_GIT_WARN_BRANCHLIMIT;
	CONFIG_GIT_REPOTYPE = CONFIG_PROMPT_GIT_REPOTYPE;
	CONFIG_GIT_REPOTYPE_PARENT = CONFIG_PROMPT_GIT_REPOTYPE_PARENT;
	CONFIG_GIT_REPONAME = CONFIG_PROMPT_GIT_REPONAME;
	CONFIG_GIT_BRANCHNAME = CONFIG_PROMPT_GIT_BRANCHNAME;
	CONFIG_GIT_BRANCH_OVERVIEW = CONFIG_PROMPT_GIT_BRANCHINFO;
	CONFIG_GIT_BRANCHSTATUS = CONFIG_PROMPT_GIT_BRANCHSTATUS;
	CONFIG_GIT_REMOTE = CONFIG_PROMPT_GIT_REMOTE;
	CONFIG_GIT_COMMIT_OVERVIEW = CONFIG_PROMPT_GIT_COMMITS;
	CONFIG_GIT_LOCALCHANGES = CONFIG_PROMPT_GIT_GITSTATUS;

#ifdef DEBUG
	for (int i = 0; i < argc; i++) {
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
			} else if (ipMode == IP_MODE_LEGACY && !CONFIG_PROMPT_NETWORK) {
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
			printf("Arg_NewRemote: >%s< (n/a)\n", Arg_NewRemote);
			fflush(stdout);
			printf("User: >%s< (%i)\n", User, User_len);
			fflush(stdout);
			printf("Host: >%s< (%i)\n", Host, Host_len);
			fflush(stdout);
			printf("Arg_TerminalDevice: >%s< (%i)\n", Arg_TerminalDevice, Arg_TerminalDevice_len);
			fflush(stdout);
			printf("Time: >%s< (%i)\n", Time, Time_len);
			fflush(stdout);
			printf("TimeZone: >%s< (%i)\n", TimeZone, TimeZone_len);
			fflush(stdout);
			printf("DateInfo: >%s< (%i)\n", DateInfo, DateInfo_len);
			fflush(stdout);
			printf("CalendarWeek: >%s< (%i)\n", CalendarWeek, CalendarWeek_len);
			fflush(stdout);
			printf("Arg_LocalIPs: >%s< (%i)\n", Arg_LocalIPs, Arg_LocalIPs_len);
			fflush(stdout);
			printf("Arg_LocalIPsAdditional: >%s< (%i)\n", Arg_LocalIPsAdditional, Arg_LocalIPsAdditional_len);
			fflush(stdout);
			printf("Arg_LocalIPsRoutes: >%s< (%i)\n", Arg_LocalIPsRoutes, Arg_LocalIPsRoutes_len);
			fflush(stdout);
			printf("Arg_ProxyInfo: >%s< (%i)\n", Arg_ProxyInfo, Arg_ProxyInfo_len);
			fflush(stdout);
			printf("Arg_PowerState: >%s< (%i)\n", Arg_PowerState, Arg_PowerState_len);
			fflush(stdout);
			printf("Arg_BackgroundJobs: >%s< (%i)\n", Arg_BackgroundJobs, Arg_BackgroundJobs_len);
			fflush(stdout);
			printf("Arg_SHLVL: >%s< (%i)\n", Arg_SHLVL, Arg_SHLVL_len);
			fflush(stdout);
			printf("Arg_SSHInfo: >%s< (%i)\n", Arg_SSHInfo, Arg_SSHInfo_len);
			fflush(stdout);
			printf("Workpath: >%s<\n", path);
			fflush(stdout);
#endif
			//taking the list of jobs as input, this counts the number of spaces (and because of the trailing space also the number of entries)
			int numBgJobs = 0;
			if (Arg_BackgroundJobs_len > 0) {
				int i = 0;
				while (Arg_BackgroundJobs[i] != 0x00) {
					if (Arg_BackgroundJobs[i] == ' ' && Arg_BackgroundJobs[i + 1] != 0x00) {
						numBgJobs++;
					}
					i++;
				}
			}

			char* numBgJobsStr;
			int numBgJobsStr_len;
			if (numBgJobs != 0) {
				if (asprintf(&numBgJobsStr, " %i Job%s", numBgJobs, numBgJobs != 1 ? "s" : "") == -1) ABORT_NO_MEMORY;
				numBgJobsStr_len = strlen_visible(numBgJobsStr);
				//transfer one byte of space over to the always there display, needed to maintain spacing in case the details do not fit.
				numBgJobsStr_len++;
				Arg_BackgroundJobs_len--;

				if (!CONFIG_PROMPT_JOB_DETAILS) {
					//if details are not desired, blank them out, but do keep the space resevation transfer (still needed for prompt spacing)
					Arg_BackgroundJobs[0] = 0x00;
					Arg_BackgroundJobs_len = 0;
				}
			} else {
				numBgJobsStr = (char*)malloc(sizeof(char));
				if (numBgJobsStr == NULL) ABORT_NO_MEMORY;
				numBgJobsStr[0] = 0x00;
				numBgJobsStr_len = 0;
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
				if (asprintf(&overridefile, "%s/forcegit", secure_getenv("ST_CFG")) == -1) ABORT_NO_MEMORY;

				bool hasOverride = (access(overridefile, F_OK) != -1);
				bool allowTesting = true;
				if (hasOverride == false) { //I only need to check the exclusions if I don't have an override
					for (int i = 0; i < numGitExclusions; i++) {
						if (StartsWith(path, GIT_EXCLUSIONS[i])) {
							allowTesting = false;
							break;
						}
					}
				} else {
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
				} else {
					ri->isGitDisabled = true;
				}
			}
			AllocUnsetStringsToEmpty(ri);

#ifdef PROFILING
			ALLOW_PROFILING_TESTPATH = false;
			timespec_get(&(profiling_timestamp[PROFILE_MAIN_PROMPT_PathGitTested]), TIME_UTC);
#endif

			char* gitSegment1_BaseMarkerStart = NULL;
			char* gitSegment2_parentRepoLoc = gitSegment1_BaseMarkerStart; //just an empty default
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
				} else if (ri->isGit) {
					if (CONFIG_GIT_REPOTYPE) {
						if (asprintf(&gitSegment1_BaseMarkerStart, " [" COLOUR_GIT_BARE "%s" COLOUR_GIT_INDICATOR "GIT%s", ri->isBare ? "BARE " : "", ri->isSubModule ? "-SM" : "") == -1) ABORT_NO_MEMORY; //[%F{006}BARE %F{002}GIT-SM
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
					} else {
						gitBranchInfo = malloc(sizeof(char));
						if (gitBranchInfo == NULL) ABORT_NO_MEMORY;
						gitBranchInfo[0] = 0x00;
					}
					char* temp1_reponame = NULL;
					char* temp2_branchname = NULL;
					char* temp3_branchoverview = NULL;
					if (asprintf(&temp1_reponame, " " COLOUR_GIT_NAME "%s", ri->RepositoryName) == -1) ABORT_NO_MEMORY;
					if (asprintf(&temp2_branchname, COLOUR_CLEAR " on " COLOUR_GIT_BRANCH "%s", ri->branch) == -1) ABORT_NO_MEMORY;
					if (asprintf(&temp3_branchoverview, "/%i+%i", ri->CountActiveBranches, ri->CountFullyMergedBranches) == -1) ABORT_NO_MEMORY;
					if (asprintf(&gitSegment3_BaseMarkerEnd, COLOUR_CLEAR "%s%s%s" COLOUR_GREYOUT "%s%s%s" COLOUR_CLEAR,
								 CONFIG_GIT_REPOTYPE ? "]" : "",
								 CONFIG_GIT_REPONAME ? temp1_reponame : "",
								 CONFIG_GIT_BRANCHNAME ? temp2_branchname : "",
								 CONFIG_GIT_BRANCH_OVERVIEW && CONFIG_GIT_BRANCHNAME ? temp3_branchoverview : "",
								 (CONFIG_GIT_BRANCHSTATUS && (CONFIG_GIT_BRANCHNAME || CONFIG_GIT_REPONAME)) ? ":" : "",
								 gitBranchInfo) == -1) ABORT_NO_MEMORY;

					if (gitBranchInfo != NULL) free(gitBranchInfo);
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
				if (gitSegment1_BaseMarkerStart == NULL) ABORT_NO_MEMORY;
				gitSegment1_BaseMarkerStart[0] = 0x00;
			}
			if (gitSegment2_parentRepoLoc == NULL) {
				gitSegment2_parentRepoLoc = malloc(sizeof(char) * 1);
				if (gitSegment2_parentRepoLoc == NULL) ABORT_NO_MEMORY;
				gitSegment2_parentRepoLoc[0] = 0x00;
			}
			if (gitSegment3_BaseMarkerEnd == NULL) {
				gitSegment3_BaseMarkerEnd = malloc(sizeof(char) * 1);
				if (gitSegment3_BaseMarkerEnd == NULL) ABORT_NO_MEMORY;
				gitSegment3_BaseMarkerEnd[0] = 0x00;
			}
			if (gitSegment4_remoteinfo == NULL) {
				gitSegment4_remoteinfo = malloc(sizeof(char) * 1);
				if (gitSegment4_remoteinfo == NULL) ABORT_NO_MEMORY;
				gitSegment4_remoteinfo[0] = 0x00;
			}
			if (gitSegment5_commitStatus == NULL) {
				gitSegment5_commitStatus = malloc(sizeof(char) * 1);
				if (gitSegment5_commitStatus == NULL) ABORT_NO_MEMORY;
				gitSegment5_commitStatus[0] = 0x00;
			}
			if (gitSegment6_gitStatus == NULL) {
				gitSegment6_gitStatus = malloc(sizeof(char) * 1);
				if (gitSegment6_gitStatus == NULL) ABORT_NO_MEMORY;
				gitSegment6_gitStatus[0] = 0x00;
			}

			gitSegment1_BaseMarkerStart_len = strlen_visible(gitSegment1_BaseMarkerStart);
			gitSegment2_parentRepoLoc_len = strlen_visible(gitSegment2_parentRepoLoc);
			gitSegment3_BaseMarkerEnd_len = strlen_visible(gitSegment3_BaseMarkerEnd);
			gitSegment4_remoteinfo_len = strlen_visible(gitSegment4_remoteinfo);
			gitSegment5_commitStatus_len = strlen_visible(gitSegment5_commitStatus);
			gitSegment6_gitStatus_len = strlen_visible(gitSegment6_gitStatus);

			int RemainingPromptWidth = Arg_TotalPromptWidth - ((CONFIG_PROMPT_USER ? User_len : 0) +
															   ((CONFIG_PROMPT_USER && CONFIG_PROMPT_HOST) ? 1 : 0) +
															   (CONFIG_PROMPT_HOST ? Host_len : 0) +
															   Arg_SHLVL_len +
															   gitSegment1_BaseMarkerStart_len +
															   gitSegment3_BaseMarkerEnd_len +
															   gitSegment5_commitStatus_len +
															   gitSegment6_gitStatus_len +
															   Time_len +
															   Arg_ProxyInfo_len +
															   numBgJobsStr_len +
															   Arg_PowerState_len + 1);

#define AdditionalElementCount 11
			uint32_t AdditionalElementAvailabilityPackedBool =
				determinePossibleCombinations(&RemainingPromptWidth, AdditionalElementCount,
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

#ifdef PROFILING
			timespec_get(&(profiling_timestamp[PROFILE_MAIN_PROMPT_PREP_COMPLETE]), TIME_UTC);
#endif

#define AdditionalElementPriorityGitRemoteInfo		 0
#define AdditionalElementPriorityLocalIP			 1
#define AdditionalElementPriorityCalendarWeek		 2
#define AdditionalElementPriorityTimeZone			 3
#define AdditionalElementPriorityDate				 4
#define AdditionalElementPriorityTerminalDevice		 5
#define AdditionalElementPriorityParentRepoLocation	 6
#define AdditionalElementPriorityBackgroundJobDetail 7
#define AdditinoalElementPriorityNonDefaultNetworks	 8
#define AdditionalElementPriorityRoutingInfo		 9
#define AdditionalElementPrioritySSHInfo			 10

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

			if (CONFIG_PROMPT_TERMINAL_DEVICE) {
				//if the fourth-prioritized element (the line/terminal device has space, append it to the user@machine) ":/dev/pts/0"
				if (AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityTerminalDevice)) {
					printf(COLOUR_TERMINAL_DEVICE ":%s", Arg_TerminalDevice);
				}
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
			for (int i = 0; i < RemainingPromptWidth; i++) {
				//this switch statement is only responsible for printing possibly neccessary escape chars
				switch (CONFIG_PROMPT_FILLER_CHAR[0]) {
					case '\\': {
						putc(CONFIG_PROMPT_FILLER_CHAR[0], stdout);
						putc(CONFIG_PROMPT_FILLER_CHAR[0], stdout);
						//intentionally missing break; as \ needs more escaping to work
					}
					case '%': {
						//since % is an escape char for the prompt string, escape it as %%
						putc(CONFIG_PROMPT_FILLER_CHAR[0], stdout);
						break;
					}
					case '$': {
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
					default: {
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

			if (CONFIG_PROMPT_NETWORK) {
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
			}

			printf("%s", numBgJobsStr);
			//if the sixth-prioritized element (background tasks) has space, print it " {1S-  2S+}"
			if ((AdditionalElementAvailabilityPackedBool & (1 << AdditionalElementPriorityBackgroundJobDetail))) {
				printf("%s", Arg_BackgroundJobs);
				if (CONFIG_PROMPT_JOBS && !CONFIG_PROMPT_JOB_DETAILS) {
					putc(' ', stdout);
				}
			} else {
				if (numBgJobs != 0) {
					putc(' ', stdout); //if the job details don't fit, use the transferred byte of space to ensure spacing
				}
			}

			//print the battery state (the first unicode char can be any of ▲,≻,▽ or ◊[for not charging,but not discharging]), while the second unicode char indicates the presence of AC power "≻ 100% ↯"
			printf("%s", Arg_PowerState);

			//the last char on screen (after any spaces contained in something) was intentionally empty(achieved by the last +1 in the calculation for RemainingPromptWidth just before the call to determinePossibleCombinations), I am now printing  ' !' there if ANY additional element had to be omitted
			printf("%s\n", ~AdditionalElementAvailabilityPackedBool & ~(~0U << AdditionalElementCount) ? "!" : "");

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
	} else if (IsLowPrompt) {
		//once again a single unicode char and escape sequence -> mark as escape sequence for ZSH with %{...%} and add %G to signify glitch (the unicode char)
		printf("%%{%%G%s %%}", CONFIG_LOWPROMPT_START_CHAR);
		if (CONFIG_LOWPROMPT_INDICATE_VENV) {
			const char* venvPrompt = secure_getenv("VIRTUAL_ENV_PROMPT");
			if (venvPrompt != NULL) {
				printf("%%{\e[35m%%}%s", venvPrompt);
			}
		}
		printf("%%{\e[36m\e[1m%%}");
		if (CONFIG_LOWPROMPT_PATH_LIMIT) {
			int chars = 0; //With max length preset -1 (half available space) a terminal would need to be less than 512 chars wide. on a very large and wide screen 512 chars is within what I consider realistically possible -> I need more bits -> uint16
			switch (CONFIG_LOWPROMPT_PATH_MAXLEN) {
				case -1: {
					chars = Arg_TotalPromptWidth / 2;
					//allow a half of the screen width to be current working directory
					break;
				}
				case -2: {
					chars = Arg_TotalPromptWidth / 3;
					//allow a third of the screen width to be current working directory
					break;
				}
				case -3: {
					chars = Arg_TotalPromptWidth / 4;
					//allow a quarter of the screen width to be current working directory
					break;
				}
				case -4: {
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
		} else {
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
	} else {
		printf("unknown command %s\n", argv[1]);
		return -1;
	}

	if (IsPrompt || IsLowPrompt) {
		Cleanup();
	}
	gitfunc_deinit();

#ifdef PROFILING
	timespec_get(&(profiling_timestamp[PROFILE_MAIN_END]), TIME_UTC);
	uint8_t lastNonNull = 0;
	for (int i = 1; i < PROFILE_MAIN_END + 1; i++) {
		if (profiling_timestamp[i].tv_nsec == 0 && profiling_timestamp[i].tv_sec == 0) {
			continue;
		} else {
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
