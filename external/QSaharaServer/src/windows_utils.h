#ifndef WINDOWS_UTILS_H
#define WINDOWS_UTILS_H

#include <ctype.h>
#include <string.h>
#include <Windows.h>
#include <direct.h>
//#include <io.h>	// for _access

struct option {
    const char *name;
    int         has_arg;
    int        *flag;
    int         val;
};
extern char *optarg;
extern int optind, opterr, optopt;
int getopt_long(int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex);

enum mode_t {
	S_IRWXU,
	S_IRWXG,
	S_IROTH,
	S_IXOTH
};
//int mkdir(const char *path, enum mode_t mode);

void gettimeofday(struct timeval *t, void* tzp);
int getpagesize(void);

#endif
