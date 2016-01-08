// Copyright unknown, presumed public domain

#if defined(_WIN32)

#include "asprintf.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

/* Some versions of MinGW are
missing _vscprintf's declaration, although they
 * still provide the symbol in the import library.
 */
#ifdef __MINGW32__
_CRTIMP int _vscprintf(const char *format, va_list argptr);
#endif

int
vasprintf(char **strp, const char *fmt, va_list args)
{
   va_list args_copy;
   int length;
   size_t size;

   va_copy(args_copy, args);

#ifdef _WIN32
   /* We need to use _vcsprintf to calculate the length as vsnprintf returns -1
    * if the number of characters to write is greater than count.
    */
   length = _vscprintf(fmt, args_copy);
#else
   char dummy;
   length = vsnprintf(&dummy, sizeof dummy, fmt, args_copy);
#endif

   va_end(args_copy);

   assert(length >= 0);
   size = length + 1;

   *strp = malloc(size);
   if (!*strp) {
       return -1;
   }

   vsnprintf(*strp, size, fmt, args);

   return length;
}

int
asprintf(char **str, const char *fmt, ...)
{
    va_list ap;
    int ret;

    *str = NULL;
    va_start(ap, fmt);
    ret = vasprintf(str, fmt, ap);
    va_end(ap);

    return ret;
}

#endif /* _WIN32 */

