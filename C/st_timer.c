#/*
TargetDir="$ST_CFG"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename "$0" .c).elf"
printf "compiling %s into %s/%s" "$0" "$TargetDir" "$TargetName"
#shellcheck disable=SC2086 #in this case I DO want word splitting to happen at $1
gcc -O3 -std=c2x -Wall "$(realpath "$(dirname "$0")")"/commons.c "$0" -o "$TargetDir/$TargetName" $1
#I WANT to be able to do things like ./repotools.c -DPROFILING to add the compiler flag profiling but ALSO stuff like ./repotools "-DDEBUG -DPROFILING" to add both profiling and debug
printf " -> \e[32mDONE\e[0m(%s)\n" $?
exit
*/

//to cross compile this for arm:
// sudo apt install gcc-12-arm-linux-gnueabi libc6-armel-cross libc6-dev-armel-cross qemu-user qemu-user-static
//compile: //arm-linux-gnueabi-gcc-12 -std=c2x -Wall ../C/commons.c timer.c -o timer
//execute: //qemu-arm -L /usr/arm-linux-gnueabi ./timer START


//the numbers this is using are UTC time (1/10 seconds), so UTC millis, but with the last two digits chopped off

#include "commons.h"
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>

void stopTimer(unsigned long long start) {
	struct timespec ts;
	timespec_get(&ts, TIME_UTC);
	uint64_t stop = ts.tv_sec * 10ULL + (ts.tv_nsec / 100000000ULL);
	uint64_t diff = stop - start;
	//printf("start: %llu\tstop: %llu\tdiff: %llu\n\n", start, stop, diff);
#define DAY_CONVERSION (10*3600*24UL)
#define HOUR_CONVERSION (10*3600UL)
#define MINUTE_CONVERSION (10*60UL)
#define SECOND_CONVERSION (10.0)
	uint32_t days = (uint32_t)(diff / DAY_CONVERSION);
	diff = diff - (days * DAY_CONVERSION);
	//printf("diff: %llu, days %i\n", diff, days);

	uint32_t hours = (uint32_t)(diff / HOUR_CONVERSION);
	diff = diff - (hours * HOUR_CONVERSION);
	//printf("diff: %llu, hours %i\n", diff, hours);

	uint32_t minutes = (uint32_t)(diff / MINUTE_CONVERSION);
	diff = diff - (minutes * MINUTE_CONVERSION);
	//printf("diff: %llu, minutes %i\n", diff, minutes);

	float seconds = (float)diff / SECOND_CONVERSION;
	if (days > 0) {
		printf("%ud ", days);
	}
	if (hours > 0) {
		printf("%uh ", hours);
	}
	if (minutes > 0) {
		printf("%um ", minutes);
	}
	printf("%.1fs", seconds);
}

int main(int argc, char** argv) {
	if (!(argc == 2 || argc == 3 || argc == 8)) {
		/*cppcheck completely looses it's mind over this.
		const char* txt = "Some String";
		//CppCheck belives the following is correct, but it obviously isn't. argv is char**, not int
		//printf("1: %1$s  2: %1$s  3: %2$i", txt, argv);
		//CppCheck believes the following is incorrect, but it definitly is.
		printf("1: %1$s  2: %1$s  3: %2$i", txt, argc);
		//gcc -with -Wall or -Wformat agrees with me. In this case if I get cppcheck to shut up, the compiler will be pissed and vice versa
		//therefore I will agree with gcc and in-line suppress cppcheck
		*/
		// cppcheck-suppress invalidPrintfArgType_s
		printf("usage: '%1$s START' OR '%1$s STOP x' where x is the output this program gave when called with START\nYou supplied %2$i arguments but only 1, 2 or 7 are valid", argv[0], argc);
		return 1;
	}
	if (Compare(argv[1], "START") && argc == 2) {
		struct timespec ts;
		timespec_get(&ts, TIME_UTC);
		printf("%li%li", ts.tv_sec, (ts.tv_nsec / 100000000L));
	}
	else if (Compare(argv[1], "STOP") && argc == 3) {
		uint64_t start = strtoull(argv[2], NULL, 10);
		stopTimer(start);
	}
	else if (Compare(argv[1], "STOPHUMAN") && argc == 8) {
		struct tm t = { 0 };
		t.tm_year = atoi(argv[2]) - 1900;	// Year minus 1900
		t.tm_mon = atoi(argv[3]) - 1;		// Month (0-11, -> really month minus 1)
		t.tm_mday = atoi(argv[4]);			// Day of the month
		t.tm_hour = atoi(argv[5]);			// Hour
		t.tm_min = atoi(argv[6]);			// Minute
		t.tm_sec = atoi(argv[7]);			// Second
		t.tm_isdst = -1;					//a negative value means that mktime() should (use timezone information and system databases to) attempt to determine whether DST is in effect at the specified time.

		time_t ut = mktime(&t);
		stopTimer(ut * 10ULL);
	}
	else {
		fprintf(stderr, "invalid argument %s or invalid argument count %i", argv[1], argc - 1);
		return 1;
	}
	return 0;
}
