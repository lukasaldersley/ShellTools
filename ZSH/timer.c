#/*
echo "compiling $0 into ~/timer-zsh.elf"
gcc -std=c2x "$0" -o ~/timer-zsh.elf
exit
(basename $0 .c) would be used to get the name of file without the .c extension
*/

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>

bool Compare(const char* a, const char* b) {
	bool matching = true;
	int idx = 0;
	while (matching && a[idx] != 0x00 && b[idx] != 0x00) {
		if (a[idx] != b[idx]) {
			matching = false;
			break;
		}
		idx++;
	}
	return matching;
}

int main(int argc, char** argv) {
	if (argc < 2 || argc>3) {
		fprintf(stderr, "usage: '%1$s START' OR '%1$s STOP x' where x is the output this program gave when called with START\nYou supplied %2$i arguments but only 1 or 2 are valid", argv[0], argc - 1);
		return 1;
	}
	if (Compare(argv[1], "START") && argc == 2) {
		struct timespec ts;
		timespec_get(&ts, TIME_UTC);
		printf("%li%li", ts.tv_sec, (ts.tv_nsec / 100000000UL));
	}
	else if (Compare(argv[1], "STOP") && argc == 3) {
		struct timespec ts;
		timespec_get(&ts, TIME_UTC);
		unsigned long long start = strtoull(argv[2], NULL, 10);
		unsigned long long stop = ts.tv_sec * 10 + (ts.tv_nsec / 100000000UL);
		unsigned long long diff = stop - start;
		//printf("start: %llu\tstop: %llu\tdiff: %llu\n\n", start, stop, diff);
#define DAY_CONVERSION (10*3600*24)
#define HOUR_CONVERSION (10*3600)
#define MINUTE_CONVERSION (10*60)
#define SECOND_CONVERSION (10.0)
		int days = diff / DAY_CONVERSION;
		diff = diff - (days * DAY_CONVERSION);
		//printf("diff: %llu, days %i\n", diff, days);

		int hours = diff / HOUR_CONVERSION;
		diff = diff - (hours * HOUR_CONVERSION);
		//printf("diff: %llu, hours %i\n", diff, hours);

		int minutes = diff / MINUTE_CONVERSION;
		diff = diff - (minutes * MINUTE_CONVERSION);
		//printf("diff: %llu, minutes %i\n", diff, minutes);

		float seconds = (float)diff / SECOND_CONVERSION;
		if (days > 0) {
			printf("%id ", days);
		}
		if (hours > 0) {
			printf("%ih ", hours);
		}
		if (minutes > 0) {
			printf("%im ", minutes);
		}
		printf("%.1fs", seconds);
	}
	else {
		fprintf(stderr, "invalid argument %s or invalid argument count %i", argv[1], argc - 1);
		return 1;
	}
	return 0;
}