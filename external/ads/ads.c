/* This is a simple wrapper for "adb shell" */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#define CMD_MAX 256

int main(int argc, void *argv[])
{
	char cmd[CMD_MAX];
	int i;
	int ret;

	if (argc < 2) {
		printf("ads \"cmd\" to execute\n");
		return -EINVAL;
	}

	sprintf(cmd, "adb shell");
	for (i=1; i<argc; i++)
		sprintf(cmd, "%s %s", cmd, argv[i]);

#ifdef DEBUG
	printf("%s", cmd);
	exit(0);
#endif

	return system(cmd);
}
