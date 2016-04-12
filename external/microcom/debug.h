#ifndef DEBUG_H  /* protection for multiple include */
#define DEBUG_H

#define STRING(x) #x
#define TO_STRING(x) STRING(x)

#ifndef DEBUG_EOL
#define DEBUG_EOL "\n"
#endif

#ifndef LOGGER
#include <syslog.h>
#define LOGGER syslog
#endif /* LOGGER */

#define DEBUG_LOG_HEADER ""
#define SIMPLE_DEBUG_LOG_HEADER DEBUG_LOG_HEADER ":"
#define DEBUG_LOG_HEADER_POS    " [ %s ("  __FILE__ ":%d)]:"

#define CRIT_MSG(fmt,...)       LOGGER(LOG_EMERG,DEBUG_LOG_HEADER_POS fmt DEBUG_EOL,__FUNCTION__,__LINE__, ##__VA_ARGS__)
#define ERROR_MSG(fmt,...)      LOGGER(LOG_ERR,DEBUG_LOG_HEADER_POS fmt DEBUG_EOL,__FUNCTION__,__LINE__, ##__VA_ARGS__)
#define WARNING_MSG(fmt,...)            LOGGER(LOG_WARNING,SIMPLE_DEBUG_LOG_HEADER fmt DEBUG_EOL, ##__VA_ARGS__)
#define NOTICE_MSG(fmt,...)             LOGGER(LOG_NOTICE,SIMPLE_DEBUG_LOG_HEADER fmt DEBUG_EOL, ##__VA_ARGS__)

#ifdef _DEBUG_

#define INFO_MSG(fmt,...)               LOGGER(LOG_INFO,SIMPLE_DEBUG_LOG_HEADER fmt DEBUG_EOL, ##__VA_ARGS__)
#define DEBUG_MSG(fmt,...)              LOGGER(LOG_DEBUG,SIMPLE_DEBUG_LOG_HEADER fmt DEBUG_EOL, ##__VA_ARGS__)
#define DEBUG_MARK                      LOGGER(LOG_DEBUG, SIMPLE_DEBUG_LOG_HEADER __FILE__ "(%d) in %s" DEBUG_EOL,__LINE__,__FUNCTION__)
#define DEBUG_VAR(x,f)                  LOGGER(LOG_DEBUG, SIMPLE_DEBUG_LOG_HEADER __FILE__ "(%d) %s: " #x " = " f DEBUG_EOL,__LINE__,__FUNCTION__,x)
#define DEBUG_VAR_BOOL(Var)             LOGGER(LOG_DEBUG, SIMPLE_DEBUG_LOG_HEADER __FILE__ "(%d) %s: " #Var " = %s" DEBUG_EOL,__LINE__,__FUNCTION__,(Var?"True":"False"))

#include <stdio.h>

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif /* MIN */

static __inline void dumpMemory(const char *memoryName,const void *address, unsigned int size)
{
    const unsigned int nbBytesPerLines = 16;
    char hexa[(nbBytesPerLines+1) * 3];
    char ascii[(nbBytesPerLines+1)];
    register const unsigned char *cursor = (unsigned char *)address;
    const unsigned char *limit = cursor + size;

    LOGGER(LOG_DEBUG,SIMPLE_DEBUG_LOG_HEADER " *** begin of memory dump of %s (size = %d bytes) ***" DEBUG_EOL,memoryName,size);
    while(cursor < limit) {
        register unsigned int i;
        register char *hexaCursor = hexa;
        register char *asciiCursor = ascii;
        const unsigned int remaining = limit-cursor;
        const unsigned int lineSize = MIN(nbBytesPerLines,remaining);

        for(i=0; i<lineSize; i++) {
            hexaCursor += sprintf(hexaCursor,"%.2X ",*cursor);
            if ((*cursor >= 0x20) && (*cursor<= 0x7A)) {
                asciiCursor += sprintf(asciiCursor,"%c",*cursor);
            } else {
                asciiCursor += sprintf(asciiCursor,".");
            }
            cursor++;
        }
        LOGGER(LOG_DEBUG, SIMPLE_DEBUG_LOG_HEADER " %s\t%s" DEBUG_EOL,hexa,ascii);
    }
    LOGGER(LOG_DEBUG,SIMPLE_DEBUG_LOG_HEADER " *** end of memory dump of %s (size = %d bytes) ***" DEBUG_EOL,memoryName,size);
}

#define DEBUG_DUMP_MEMORY(Var,Size) dumpMemory(#Var,Var,Size)

#else

#define INFO_MSG(fmt,...)
#define DEBUG_MSG(fmt,...)
#define DEBUG_MARK
#define DEBUG_VAR(x,f)
#define DEBUG_VAR_BOOL(Var)
#define DEBUG_DUMP_MEMORY(Var,Size)

#endif /* _DEBUG_ */

#endif /* DEBUG_H */
