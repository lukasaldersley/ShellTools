#/*
printf "compiling $0 into ~/timer-zsh.elf"
gcc -std=c2x -Wall "$(realpath "$(dirname "$0")/../")"/C/commons.c "$0" -o ~/timer-zsh.elf
printf " -> \e[32mDONE\e[0m($?)\n"
exit
(basename $0 .c) would be used to get the name of file without the .c extension
*/

//to cross compile this for arm:
// sudo apt install gcc-12-arm-linux-gnueabi libc6-armel-cross libc6-dev-armel-cross qemu-user qemu-user-static
//compile: //arm-linux-gnueabi-gcc-12 -std=c2x -Wall ../C/commons.c timer.c -o timer
//execute: //qemu-arm -L /usr/arm-linux-gnueabi ./timer START

#include "../C/commons.h"
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>

int main(int argc, char** argv) {
	if (argc < 2 || argc>3) {
		fprintf(stderr, "usage: '%1$s START' OR '%1$s STOP x' where x is the output this program gave when called with START\nYou supplied %2$i arguments but only 1 or 2 are valid", argv[0], argc - 1);
		return 1;
	}
	if (Compare(argv[1], "START") && argc == 2) {
		struct timespec ts;
		timespec_get(&ts, TIME_UTC);
		printf("%li%li", ts.tv_sec, (ts.tv_nsec / 100000000L));
	}
	else if (Compare(argv[1], "STOP") && argc == 3) {
		struct timespec ts;
		timespec_get(&ts, TIME_UTC);
		uint64_t start = strtoull(argv[2], NULL, 10);
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
	else {
		fprintf(stderr, "invalid argument %s or invalid argument count %i", argv[1], argc - 1);
		return 1;
	}
	return 0;
}
