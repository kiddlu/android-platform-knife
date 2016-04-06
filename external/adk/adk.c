/* Android Debug Kit */
/* This is a simple wrapper for "adb function / shell" */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

//#define DEBUG
#define CMD_MAX 256

int main(int argc, char **argv)
{
	char cmd[CMD_MAX];
	int i;

    argc -= optind;
    argv += optind;

	if (argc < 1) {
		printf("adk \"cmd\" to execute\n");
		return -EINVAL;
	} /* build-in cmd */
	else if(!strcmp(*argv, "ftyrst")) {
		sprintf(cmd, "adb shell am broadcast -a android.intent.action.MASTER_CLEAR");
		goto execute;
	} else if(!strcmp(*argv, "pk")) {
		if ( argc < 2 ) {
			printf("pls set pid to kill");
			return -EINVAL;
		}
		sprintf(cmd, "adb shell kill -9");
		argc -= 1;
		argv += 1;
		goto construct;
	} else if(!strcmp(*argv, "smartisan-active")) {
		sprintf(cmd, "adb shell am start -n com.smartisanos.setupwizard/com.smartisanos.setupwizard.SetupWizardCompleteActivity");
		goto execute;
	} else if(!strncmp(*argv, "xxxxxxxxxxxxxxxx", strlen("xxxxxxxxxxx"))) {
		return 0;
	} else {
		sprintf(cmd, "adb shell");
	}

construct:
	for (i=0; i<argc; i++)
		sprintf(cmd, "%s %s", cmd, argv[i]);

execute:
#ifdef DEBUG
	printf("%s", cmd);
#endif
	return system(cmd);
}