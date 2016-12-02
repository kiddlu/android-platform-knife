#ifndef EXTERNAL_UTILS_H
#define EXTERNAL_UTILS_H

#if defined(LINUXPC) || defined(WINDOWSPC)

#include <stdlib.h>
#include <string.h>

size_t strlcpy(char *dst, const char *src, size_t size);
size_t strlcat(char *dst, const char *src, size_t size);

#endif

#endif

