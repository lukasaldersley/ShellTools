#/*
TargetDir="$HOME/.shelltools"
if [ ! -d "$TargetDir" ]; then
	mkdir -p "$TargetDir"
fi
TargetName="$(basename $0 .c).elf"
printf "compiling $0 into $TargetDir/$TargetName"
gcc -O3 -std=c2x -Wall "$(realpath "$(dirname "$0")")"/commons.c "$0" -o "$TargetDir/$TargetName"
printf " -> \e[32mDONE\e[0m($?)\n"
exit
*/

/*
The reason this even exists is Microsoft's shoddy coding.
Windows often complains you need to obtain permissions from <your own user> to delete a folder if that folder still contains files (also owned by your own user).
You then have to first delete the files and only then can you delete the parent folder.
Since this is really annoying if you're dealing with hundreds of subfolders with many dozen subfolders each and approximately 150k files, I created this to do that automatically.
*/

#include "commons.h"
#include <dirent.h>

void func(const char* dir) {
	printf("start recursive rm on %s\n", dir);
	DIR* directoryPointer;
	struct dirent* direntptr;
	directoryPointer = opendir(dir);
	if (directoryPointer != NULL)
	{
		while ((direntptr = readdir(directoryPointer)))
		{
			//printf("testing: %s (directory: %s)\n", direntptr->d_name, (direntptr->d_type == DT_DIR ? "TRUE" : "NO"));
			if (Compare(direntptr->d_name, ".") || Compare(direntptr->d_name, "..")) {
				continue;
			}
			else {
				char* next;
				if (asprintf(&next, "%s/%s", dir, direntptr->d_name) == -1) {
					fprintf(stderr, "Out of memory\n");
					exit(1);
				}
				if (direntptr->d_type == DT_DIR) {
					func(next);
				}
				else {
					printf("deleting sub-element: %s\n", next);
					remove(next);
				}
			}
		}
		closedir(directoryPointer);
		printf("deleting parentfolder: %s\n", dir);
		remove(dir);
	}
	else
	{
		fprintf(stderr, "failed on directory: %s\n", dir);
		perror("Couldn't open the directory\n");
	}
}

int main(int argc, char** argv) {
	if (argc == 2) {
		func(argv[1]);
		return 0;
	}
	else {
		fprintf("you must supply exactly one argument\n");
		return 1;
	}
}
