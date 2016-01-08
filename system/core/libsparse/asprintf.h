#ifndef ASPRINTF_H_
#define ASPRINTF_H_
#if defined(_WIN32)
#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

int vasprintf(char **strp, const char *fmt, va_list args);
int asprintf(char **strp, const char *fmt, ...);

#ifdef __cplusplus
}
#endif
#endif /* _WIN32 */
#endif /* ASPRINTF_H_ */
