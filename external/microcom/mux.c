/***************************************************************************
** File: mux.c
** Description: the main program loop
**
** Copyright (C)1999 Anca and Lucian Jurubita <ljurubita@hotmail.com>.
** All rights reserved.
****************************************************************************
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details at www.gnu.org
****************************************************************************
** Rev. 1.0 - Feb. 2000
****************************************************************************/
#include "microcom.h"

#define SCRIPT_DELAY 1
#define BUFSIZE 1024

extern int script;
extern char scr_name[];
extern FILE* flog;
extern unsigned int options;

void mux_clear_sflag(void)
{
    script = 0;
}

static inline char *PrintableBuffer(const char *buffer,const size_t l, size_t *size)
{
    static char printable[BUFSIZE+1];
    const char *limit = buffer + l;
    const char *i = buffer;
    char *j = printable;
    for(i=buffer; i<limit; i++) {
        if (isprint(*i)) {
            *j = *i;
            j++;
        }
    }
    *size = j - printable;
    *j = '\0';
    return printable;
}

/* main program loop */
void mux_loop(int pf)
{
    fd_set  ready;        /* used for select */
    int     i = 0;        /* used in the multiplex loop */
    int     done = 0;
    char    buf[BUFSIZE];
    struct timeval tv;

    tv.tv_sec = SCRIPT_DELAY;
    tv.tv_usec = 0;

    if (script) {
        script_init(scr_name);
    }

    do { /* forever */
        FD_ZERO(&ready);
        FD_SET(STDIN_FILENO, &ready);
        FD_SET(pf, &ready);

        if (script) {
            if (!select(pf+1, &ready, NULL, NULL, &tv)) {
                i = script_process(S_TIMEOUT, buf, BUFSIZE);
                if (i > 0) {
                    cook_buf(pf, buf, i);
                }
                /* restart timer */
                tv.tv_sec = SCRIPT_DELAY;
                tv.tv_usec = 0;
            }
        } /* if */
        else {
            select(pf+1, &ready, NULL, NULL, NULL);
        }

        if (FD_ISSET(pf, &ready)) {
            /* pf has characters for us */
            i = read(pf, buf, BUFSIZE);
            if (i > 0) {
                if (options & OPTION_LOG_FILTER) {
                    /* only printable characters */
                    size_t size = 0;
                    char *printable = PrintableBuffer(buf,i,&size);
                    DEBUG_MSG("received printable buffer = %s",printable);
                    if (flog != NULL) {
                        printable[size] = '\n';
                        printable[size+1] = '\0';
                        const size_t written = fwrite(printable, 1, size, flog);
                        if (written != size) {
                            const int error = errno;
                            DEBUG_MSG("error writing log file, only %u characters written, errno = %d",written,error);
                        }
                    }
                } else {
                    /* raw memory dump */
                    DEBUG_DUMP_MEMORY(buf,i);
                    if (flog != 0) {
                        fwrite(buf, 1, i, flog);
                    }
                }
                write(STDOUT_FILENO, buf, i);
                if (script) {
                    i = script_process(S_DCE, buf, i);
                    if (i > 0) {
                        cook_buf(pf, buf, i);
                    }
                }
            } else {
                done = 1;
            }
        } /* if */

        if (FD_ISSET(STDIN_FILENO, &ready)) {
            /* standard input has characters for us */
            i = read(STDIN_FILENO, buf, BUFSIZE);
            if (i > 0) {
                cook_buf(pf, buf, i);
            } else {
                done = 1;
            }
        } /* if */
    } while (!done); /* do */
}




