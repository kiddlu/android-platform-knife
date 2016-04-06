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
		printf("Android Debug Kit\n");
		printf("\nadk \"cmd\" to execute\n");
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
	} else if(!strcmp(*argv, "meminfo")) {
		unsigned int sleep_time = 0;
		unsigned int loop_time = 0;
		if ( argc == 1 ) {
			sleep_time = 0;
		} else {
			sleep_time = atoi(argv[1]);
		}
		do {
			loop_time++;
			sprintf(cmd, "adb shell echo ======loop times:%d sleep:%d======", loop_time, sleep_time);
			system(cmd);

			system("adb shell cat /proc/meminfo");
			system("adb shell cat /proc/pagetypeinfo");
			system("adb shell cat /proc/slabinfo");
			system("adb shell cat /proc/zoneinfo");
			system("adb shell cat /proc/vmallocinfo");
			system("adb shell cat /proc/vmstat");
			system("adb shell cat /proc/meminfo");
			system("adb shell top -n 1");
			system("adb shell free -m");

			sprintf(cmd, "adb shell sleep %d", sleep_time);
			system(cmd);
		} while(sleep_time);
		return 0;
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