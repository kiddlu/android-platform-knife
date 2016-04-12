/***************************************************************************
** File: script.c
** Description: script state machine
****************************************************************************
** Original code for script.c, modified and ported to microcom
**
** Runscript    Run a login-or-something script.
**      A basic like "programming language".
**      This program also looks like a basic interpreter :
**      a bit messy. (But hey, I'm no compiler writer :-))
**
** Version: 1.21
**
** Author:  Miquel van Smoorenburg, miquels@drinkel.ow.nl
**
** Bugs:    The "expect" routine is, unlike gosub, NOT reentrant !
**
**      This file is part of the minicom communications package,
**      Copyright 1991-1995 Miquel van Smoorenburg.
**
**      This program is free software; you can redistribute it and/or
**      modify it under the terms of the GNU General Public License
**      as published by the Free Software Foundation; either version
**      2 of the License, or (at your option) any later version.
******************************************************************************
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details at www.gnu.org
**
** Copyright (C)1999 Anca and Lucian Jurubita <ljurubita@hotmail.com>.
** All rights reserved.
*****************************************************************************
** Rev. 1.0 - Feb. 2000
** Rev. 1.02 - June 2000
** Rev. 1.03 - Feb. 2013
** Rev. 1.04 - Feb. 2013
** Rev. 1.05 - Mar. 2013
****************************************************************************/
#include "microcom.h"
#include "script.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>

extern int script;

/* public functions */
void script_init(char* s);
int script_process(S_ORIGINATOR orig, char* buf, int size);

/* static functions */
static int readscript(char *s);
static void skipspace(char **s);
static void freemem();
static char *strsave(char *s);
static char* script_do_line(void);
static char* s_exec(char *text);
static int getnum(char *text);
static char *getword(char **s);
static char *getstring(char **s); /* same as getword but send an empty string instead of NULL when the word is an empty string */
static VAR *getvar(char *name, int cr);
static void syntaxerr(char *s);

unsigned int timeout = DEFAULT_TIMEOUT;
static int exitOnTimeOut = TRUE;
static int verbose = FALSE;

ENV *curenv;        /* Execution environment */
#define INBUF_SIZE 1024
#define OUTBUF_SIZE 255
char in_buffer[INBUF_SIZE];
int in_index = 0;


/***********************************************************
 error reporting functions
***********************************************************/
static void s_error(fmt, a1, a2, a3)
char *fmt;
char *a1, *a2, *a3;
{
    fprintf(stderr, fmt, a1, a2, a3);
}

static void syntaxerr(char *s)
{
    s_error("script \"%s\": syntax error in line %d %s\r\n", curenv->scriptname,
            curenv->thisline->lineno, s);
    cleanup_termios(0);
}


/***********************************************************
 interface to external world
************************************************************/
/* init the script environment */
void script_init(char* s)
{
    if (curenv != NULL) {
        freemem();
        free(curenv);
    }

    curenv = (ENV*)malloc(sizeof(ENV));
    if (NULL == curenv) {
        CRIT_MSG("failed to allocate %d bytes for execution environment",sizeof(ENV));
        cleanup_termios(1); /* aborting... */
    }

    curenv->thisline = NULL;
    curenv->in_timeout = 0;
    curenv->lines = LNULL;
    curenv->vars  = VNULL;
    curenv->scriptname = s;

    if (readscript(s) < 0) {
        freemem();
        free(curenv);
    }
    DEBUG_MSG("script_init done");
}

int script_process(S_ORIGINATOR orig, char* buf, int size)
{
    char* ptr;

    switch (orig) {
    case S_TIMEOUT:      /* run next line in the script */
        buf[0] = '\0';
        ptr = script_do_line();
        if (ptr != NULL) {
            strcat(buf, ptr);
            strcat(buf, "\n");
        }
        break;
    case S_DCE:          /* store the input in in_buffer */
        if (size > 0) {
            if (in_index + size >= INBUF_SIZE) {
                in_index = 0;
            }
            strncpy(&in_buffer[in_index], buf, size);
            in_index += size;
        }

        /* return buf as is */
        buf[0] = '\0';
        break;
    default:
        break;
    }

    return strlen(buf);
}

/**********************************************************
 script interpreter
**********************************************************/
static char* script_do_line(void)
{
    char* ptr;
    if (curenv->thisline == NULL) {
        curenv->thisline = curenv->lines;
    }
    ptr = s_exec(curenv->thisline->line);
    if (curenv->in_timeout == 0) {
        curenv->thisline = curenv->thisline->next;
    }
    return ptr;
}

char* doprint(char *text)
{
    fprintf(stderr, "%s\n", text);
    return NULL;
}

/*
 * Declare a variable (integer)
 */
char* doset(char *text)
{
    char *w;
    VAR *v;

    w = getword(&text);
    if (w == CNULL) {
        syntaxerr("(missing var name)");
    }
    v = getvar(w, 1);
    if (*text) {
        v->value = getnum(getword(&text));
    }
    return NULL;
}

/*
 * Lower the value of a variable.
 */
char* dodec(char *text)
{
    char *w;
    VAR *v;

    w = getword(&text);
    if (w == CNULL)  {
        syntaxerr("(expected variable)");
    }
    v = getvar(w, 0);
    v->value--;
    DEBUG_VAR(v->value,"%d");
    return NULL;
}

/*
 * Increase the value of a variable.
 */
char* doinc(char *text)
{
    char *w;
    VAR *v;

    w = getword(&text);
    if (w == CNULL)  {
        syntaxerr("(expected variable)");
    }
    v = getvar(w, 0);
    v->value++;
    DEBUG_VAR(v->value,"%d");
    return NULL;
}

char* dosend(char *text)
{
    INFO_MSG("sending %s...",text);
    return text;
}

char* dosuspend(char *text)
{
    char *w;
    int val;

    if (curenv->in_timeout == 0) {
        w = getword(&text);
        if(w == CNULL) {
            syntaxerr("(argument expected)");
        }
        if ((val = getnum(w)) == 0) {
            syntaxerr("(invalid argument)");
        }
        curenv->in_timeout = val;
    } else {
        curenv->in_timeout--;
    }

    return NULL;
}

char* doexpect(char *text)
{
    char temp[INBUF_SIZE];
    int i;

    if (curenv->in_timeout == 0) {
        sprintf(temp, "Start waiting for \"%s\"", text);
        DEBUG_MSG("%s",temp);
        if (verbose) {
            doprint(temp);
        }
        curenv->in_timeout = timeout;
    } else {
        curenv->in_timeout--;
    }

    if (verbose) {
        if (curenv->in_timeout % 10 == 0) { /* progress */
            INFO_MSG("remaining time before time out (%d) waiting for \"%s\"", curenv->in_timeout,text);
            sprintf(temp, "timeout = %d", curenv->in_timeout);
            doprint(temp);
        }
    }

    /* clean in_buffer of '\0' */
    for (i = 0; i < in_index; i++) {
        if (in_buffer[i] == '\0') {
            in_buffer[i] = ' ';
        }
    }
    in_buffer[in_index] = '\0';

    /* look for our string */
    const char *found = strstr(in_buffer, text);
    if (found != NULL) {
        DEBUG_MSG("expected string \"%s\" found in \"%s\" !",text,in_buffer);
        curenv->in_timeout = 0;
        in_buffer[0] = '\0';
        in_index = 0;
        return NULL;
    }

    /* timeout */
    if (curenv->in_timeout == 0) {
        in_buffer[0] = '\0';
        in_index = 0;
        sprintf(temp, "Error expecting string \"%s\"", text);
        if (exitOnTimeOut) {
            mux_clear_sflag();
            NOTICE_MSG("expect timeout, script ended");
        }
        return doprint(temp);
    }

    /* nothing arrived, just clean the in_buffer; keep the last part of the buffer  */
    if (strlen(in_buffer) > strlen(text)) {
        strcpy(temp, &in_buffer[in_index - strlen(text)]);
        strcpy(in_buffer, temp);
        in_index = strlen(in_buffer);
    }
    return NULL;
}

char* dosendif(char *text)
{
    char temp[INBUF_SIZE];
    char expected[INBUF_SIZE];
    static char toSend[OUTBUF_SIZE];
    int i;
    char *word = CNULL;

    word = getword(&text);
    if (word != CNULL) {
        strcpy(expected,word);
    } else {
        syntaxerr("(sendif first args)");
    }

    word = getstring(&text); /* empty string allowed */
    if (word != CNULL) {
        strcpy(toSend,word);
    } else {
        syntaxerr("(sendif second args)");
    }

    if (curenv->in_timeout == 0) {
        sprintf(temp, "Start waiting for \"%s\" to send \"%s\"", expected,toSend);
        DEBUG_MSG("%s",temp);
        if (verbose) {
            doprint(temp);
        }
        curenv->in_timeout = timeout;
    } else {
        curenv->in_timeout--;
    }

    if (verbose) {
        if (curenv->in_timeout % 10 == 0) { /* progress message */
            sprintf(temp,"remaining time before time out (%d) waiting for \"%s\" to send \"%s\"", curenv->in_timeout,expected,toSend);
            DEBUG_MSG("%s",temp);
            doprint(temp);
        }
    }

    /* clean in_buffer of '\0' */
    for (i = 0; i < in_index; i++) {
        if (in_buffer[i] == '\0') {
            in_buffer[i] = ' ';
        }
    }
    in_buffer[in_index] = '\0';

    /* look for our string */
    const char *found = strstr(in_buffer, expected);
    if (found != NULL) {
        DEBUG_MSG("expected string \"%s\" found in \"%s\" ! => send \"%s\"",expected,in_buffer,toSend);
        curenv->in_timeout = 0;
        in_buffer[0] = '\0';
        in_index = 0;
        return toSend;
    }

    /* timeout */
    if (curenv->in_timeout == 0) {
        in_buffer[0] = '\0';
        in_index = 0;
        sprintf(temp, "Error expecting string \"%s\" to send \"%s\"", expected,toSend);
        WARNING_MSG("%s",temp);
        if (exitOnTimeOut) {
            mux_clear_sflag();
            NOTICE_MSG("sendif timeout, script ended");
        }
        return doprint(temp);
    }

    /* nothing arrived, just clean the in_buffer; keep the last part of the buffer  */
    if (strlen(in_buffer) > strlen(text)) {
        strcpy(temp, &in_buffer[in_index - strlen(text)]);
        strcpy(in_buffer, temp);
        in_index = strlen(in_buffer);
    }
    return NULL;
}

char* doexit(char* text)
{
    mux_clear_sflag();
    doprint("Script ended");
    NOTICE_MSG("script ended");
    return NULL;
}

char* doshell(char* text)
{
    char *w = getword(&text);
    if(w != CNULL) {
        system(w);
    } else {
        syntaxerr("(expected argument after !)");
    }
    return NULL;
}

/*
 * If syntax: if n1 [><=] n2 command.
 */
char* doif(char* text)
{
    char *w;
    int n1;
    int n2;
    char op;

    if ((w = getword(&text)) == CNULL) {
        syntaxerr("(if)");
    }
    n1 = getnum(w);
    if ((w = getword(&text)) == CNULL) {
        syntaxerr("(if)");
    }
    if (strcmp(w, "!=") == 0) {
        op = '!';
    } else {
        if (*w == 0 || w[1] != 0) {
            syntaxerr("(if)");
        }
        op = *w;
    }
    if ((w = getword(&text)) == CNULL) {
        syntaxerr("(if)");
    }
    n2 = getnum(w);
    if (!*text) {
        syntaxerr("(expected command after if)");
    }

    switch (op) {
    case '=':
        if (!(n1 == n2)) {
            return(OK);
        }
        break;
    case '!':
        if (!(n1 != n2)) {
            return(OK);
        }
        break;
    case '>':
        if (!(n1 > n2)) {
            return(OK);
        }
        break;
    case '<':
        if (!(n1 < n2)) {
            return(OK);
        }
        break;
    default:
        syntaxerr("(unknown operator)");
    } /* swich */

    return(s_exec(text));
}

char* dogoto(char* text)
{
    char *w;
    struct line *l;
    char buf[32];
    int len;

    w = getword(&text);
    if (w == CNULL || *text) {
        syntaxerr("(in goto/gosub label)");
    }
    snprintf(buf, sizeof(buf), "%s:", w);
    len = strlen(buf);
    for(l = curenv->lines; l; l = l->next) {
        if (!strncmp(l->line, buf, len))
            break;
    }
    if (l == LNULL) {
        s_error("script \"%s\" line %d: label \"%s\" not found\r\n",
                curenv->scriptname, curenv->thisline->lineno, w);
        cleanup_termios(0);
    }
    curenv->thisline = l;
    /* We return break, to automatically break out of expect loops. */
    return NULL;
}

char* dotimeout(char* text)
{
    char *w = getword(&text);
    if(w == CNULL) {
        syntaxerr("(argument expected)");
    } else {
        const int val = getnum(w);
        if (val > 0) {
            timeout = val;
            NOTICE_MSG("new time out value set to %u s",timeout);
        } else {
            timeout = DEFAULT_TIMEOUT;
            NOTICE_MSG("new time out value set to default value (%u s)",timeout);
        }
    }

    return NULL;
}

char* doexitOnTimeOut(char* text)
{
    char *w = getword(&text);
    if(w == CNULL) {
        exitOnTimeOut != exitOnTimeOut;
    } else {
        /* set false if the parameters is FALSE, False, false, 0,... else true */
        exitOnTimeOut = (('f' == w[0]) || ('F' == w[0]) || ('0' == w[0]))?FALSE:TRUE;
    }
    DEBUG_VAR_BOOL(exitOnTimeOut);
    return NULL;
}

char* doexitProgram(char* text)
{
    int exitCode = EXIT_SUCCESS;
    char *w = getword(&text);
    if(w != CNULL) {
        exitCode = getnum(w);
    }
    exit_program(exitCode);
    return NULL;
}

char* doverbose(char* text)
{
    char *w = getword(&text);
    if(w == CNULL) {
        verbose != verbose;
    } else {
        /* set false if the parameters is FALSE, False, false, 0,... else true */
        verbose = (('f' == w[0]) || ('F' == w[0]) || ('0' == w[0]))?FALSE:TRUE;
    }
    DEBUG_VAR_BOOL(verbose);
    return NULL;
}

/* KEYWORDS */
struct kw {
    char *command;
    char* (*fn)();
} keywords[] = {
    { "expect",   doexpect },
    { "send", dosend },
    { "suspend",  dosuspend },
    { "!",    doshell },
    { "goto", dogoto },
//  { "gosub",  dogosub },
//  { "return", doreturn },
    { "exit", doexit },
    { "quit", doexitProgram },
    { "print",    doprint },
    { "set",  doset },
    { "inc",  doinc },
    { "dec",  dodec },
    { "if",   doif },
    { "timeout",  dotimeout },
    { "sendif",   dosendif },
    { "timeoutexit" ,doexitOnTimeOut },
    { "verbose",  doverbose },
//  { "sleep",  dosleep },
//  { "break",  dobreak },
//  { "call",   docall },
    { (char *)0,  (char*(*)())0 }
};

/*
 * Execute one line.
 */
static char* s_exec(char *text)
{
    char *w;
    struct kw *k;

    w = getword(&text);

    /* If it is a label or a comment, skip it. */
    if (w == CNULL || *w == '#' || w[strlen(w) - 1] == ':') {
        return(NULL);
    }

    /* See which command it is. */
    for(k = keywords; k->command; k++) {
        if (!strcmp(w, k->command)) break;
    }

    /* Command not found? */
    if (k->command == (char *)NULL) {
        s_error("script \"%s\" line %d: unknown command \"%s\"\r\n",
                curenv->scriptname, curenv->thisline->lineno, w);
        cleanup_termios(0);
    }
    return((*(k->fn))(text));
}

/****************************************************************************
 misceleanus library functions
****************************************************************************/

/*
 * Find a variable in the list.
 * If it is not there, create it.
 */
static VAR *getvar(char *name, int cr)
{
    VAR *v, *end = VNULL;

    for(v = curenv->vars; v; v = v->next) {
        end = v;
        if (!strcmp(v->name, name)) return(v);
    }
    if (!cr) {
        s_error("script \"%s\" line %d: unknown variable \"%s\"\r\n",
                curenv->scriptname, curenv->thisline->lineno, name);
        cleanup_termios(1);
    }
    if ((v = (VAR *)malloc(sizeof(VAR))) == VNULL ||
            (v->name = strsave(name)) == CNULL) {
        s_error("script \"%s\": out of memory\r\n", curenv->scriptname);
        cleanup_termios(1);
    }
    if (end) {
        end->next = v;
    } else {
        curenv->vars = v;
    }
    v->next = VNULL;
    v->value = 0;
    return(v);
}

static int getnum(char *text)
{
    int val;

    if ((val = atoi(text)) != 0 || *text == '0') {
        return(val);
    }
    return(getvar(text, 0)->value);
}

/* read the script file */
static int readscript(char *s)
{
    FILE *fp;
    LINE *tl, *prev = LNULL;
    char *t;
    char buf[301];
    int no = 0;

    if ((fp = fopen(s, "r")) == (FILE *)0) {
        const int error = errno;
        s_error("runscript: couldn't open %s error %d (%m)\r\n", s,error);
        cleanup_termios(1);
    }

    /* Read all the lines into a linked list in memory. */
    while((t = fgets(buf, 300, fp)) != CNULL) {
        no++;
        skipspace(&t);
        if (*t == '\n') continue;
        if (  ((tl = (LINE*)malloc(sizeof (LINE))) == LNULL) ||
                ((tl->line = strsave(t)) == CNULL)) {
            s_error("script %s: out of memory\r\n", s);
            cleanup_termios(1);
        }
        if (prev) {
            prev->next = tl;
        } else {
            curenv->lines = tl;
        }
        tl->next = (LINE*)0;
        tl->labelcount = 0;
        tl->lineno = no;
        prev = tl;
    }
    return(0);
}

/* skip blank space in the string */
static void skipspace(char **s)
{
    while(**s == ' ' || **s == '\t') (*s)++;
}

/* free all malloced memory */
static void freemem()
{
    LINE *l, *nextl;
    VAR *v, *nextv;

    for(l = curenv->lines; l; l = nextl) {
        nextl = l->next;
        free((char *)l->line);
        free((char *)l);
    }
    for(v = curenv->vars; v; v = nextv) {
        nextv = v->next;
        free(v->name);
        free((char *)v);
    }
    curenv->thisline = NULL;
}

/* Save a string to memory. Strip trailing '\n' */
static char *strsave(char *s)
{
    char *t;
    int len;

    len = strlen(s);
    if (len && s[len - 1] == '\n') {
        s[--len] = 0;
    }
    if ((t = (char *)malloc(len + 1)) == (char *)0) {
        return(t);
    }
    strcpy(t, s);
    return(t);
}

/*
 * functions for special env. variables
 * man netdevice for details
 */

static inline const char *getMACAddress(const unsigned int boardNumber)
{
    static char address[18] = {'\0'} ;
    static unsigned int number = (unsigned int)(-1);

    if (boardNumber != number) {
        address[0] = '\0';
    }
    if ('\0' == address[0]) { /* only once for performance reasons */
        int error = EXIT_SUCCESS;
        int sock = socket(PF_INET, SOCK_DGRAM, 0);
        if (sock != -1) {
            struct ifreq buffer;
            memset(&buffer, 0x00, sizeof(buffer));
            sprintf(buffer.ifr_name,"eth%d",boardNumber);
            /* Get hardware address */
            if (ioctl(sock, SIOCGIFHWADDR, &buffer) != -1) {
                char *cursor = address;
                int i;
                for(i=0; i<6; i++) {
                    cursor += sprintf(cursor,"%.2X:",(unsigned char)buffer.ifr_hwaddr.sa_data[i]);
                }
                *(cursor-1) = '\0';
                DEBUG_VAR(address,"%s");
                number = boardNumber;
            } else {
                error = errno;
                ERROR_MSG("ioctl SIOCGIFHWADDR error %d (%m)\n",error);
                address[0] = '\0';
            }
            close(sock);
            sock = -1;
        } else {
            error = errno;
            ERROR_MSG("socket PF_INET error %d (%m)\n",error);
            address[0] = '\0';
        }
    }
    return address;
}

static inline const char *getIPAddress(const unsigned int boardNumber)
{
    static char address[17] = {'\0'};
    static unsigned int number = (unsigned int)(-1);

    if (boardNumber != number) {
        address[0] = '\0';
    }
    if ('\0' == address[0]) { /* only once for performance reasons */
        int error = EXIT_SUCCESS;
        int sock = socket(PF_INET, SOCK_DGRAM, 0);
        if (sock != -1) {
            struct ifreq buffer;
            memset(&buffer, 0x00, sizeof(buffer));
            sprintf(buffer.ifr_name,"eth%d",boardNumber);
            /* Get first IP address */
            if (ioctl(sock, SIOCGIFADDR, &buffer) != -1) {
                char *cursor = address;
                int i;
                for(i=2; i<6; i++) {
                    cursor += sprintf(cursor,"%.3u.",(unsigned char)buffer.ifr_addr.sa_data[i]);
                }
                *(cursor-1) = '\0';
                DEBUG_VAR(address,"%s");
                number = boardNumber;
            } else {
                error = errno;
                ERROR_MSG("ioctl SIOCGIFADDR error %d (%m)\n",error);
                address[0] = '\0';
            }
            close(sock);
            sock = -1;
        } else {
            error = errno;
            ERROR_MSG("socket PF_INET error %d (%m)\n",error);
            address[0] = '\0';
        }
    }
    return address;
}

static char *getstring(char **s)
{
    static char buf[81];
    int len, f;
    int idx = 0;
    char *t = *s;
    int sawesc = 0;
    int sawq = 0;

    for(len = 0; len < 81; len++) {
        if (sawesc && t[len]) {
            sawesc = 0;
            if (t[len] <= '7' && t[len] >= '0') {
                buf[idx] = 0;
                for(f = 0; f < 4 && len < 81 && t[len] <= '7' &&
                        t[len] >= '0'; f++)
                    buf[idx] = 8*buf[idx] + t[len++] - '0';
                if (buf[idx] == 0) buf[idx] = '@';
                idx++;
                len--;
                continue;
            }
            switch(t[len]) {
            case 'r':
                buf[idx++] = '\r';
                break;
            case 'n':
                buf[idx++] = '\n';
                break;
            case 'b':
                buf[idx++] = '\b';
                break;
#ifndef _HPUX_SOURCE
            case 'a':
                buf[idx++] = '\a';
                break;
#endif
            case 'f':
                buf[idx++] = '\f';
                break;
            case 'c':
                buf[idx++] = 255;
                break;
                /* extensions to be able to send specials values    */
            case '@':
                break;
            default:
                buf[idx++] = t[len];
                break;
            }
            sawesc = 0;
            continue;
        }
        if (t[len] == '\\') {
            sawesc = 1;
            continue;
        }
        if (t[len] == '"') {
            if (sawq == 1) {
                sawq = 0;
                len++;
                break;
            }
            sawq = 1;
            continue;
        }
        if (t[len] == '$' && t[len + 1] == '(') {
            for(f = len; t[f] && t[f] != ')'; f++);
            if (t[f] == ')') {
                const char *env  = NULL;
                char envbuf[32];
                strncpy(envbuf, &t[len + 2], f - len - 2);
                envbuf[f - len - 2] = 0;
                len = f;
                if (strncmp(envbuf,"@IP",3) == 0) {
                    const unsigned int boardNumber = (envbuf[3] != '\0')? *(envbuf + 3) - '0' : 0;
                    env = getIPAddress(boardNumber);
                    if ('\0' == env[0]) {
                        /* work around:
                         * may fail if the two network interfaces are directly linked and the remote one is temporary down
                         * try again once
                         */
                        sleep(2);
                        env = getIPAddress(boardNumber);
                    }
                } else if (strncmp(envbuf,"@MAC",4) == 0) {
                    const unsigned int boardNumber = (envbuf[4] != '\0')? *(envbuf + 4) - '0' : 0;
                    env = getMACAddress(boardNumber);
                    /* work around:
                     * may fail if the two network interfaces are directly linked and the remote one is temporary down
                     * try again once
                     */
                    sleep(2);
                    env = getIPAddress(boardNumber);
                } else {
                    ;
                    if ((env = getenv(envbuf)) == NULL) {
                        env = "";
                    }
                }
                DEBUG_VAR(env,"%s");
                while((*env) && (idx < sizeof(buf)-1)) {
                    buf[idx++] = *env++;
                }
                continue;
            }
        }
        if((!sawq && (t[len] == ' ' || t[len] == '\t')) || t[len] == '\0') break;
        buf[idx++] = t[len];
    }
    buf[idx] = 0;
    (*s) += len;
    skipspace(s);
    if (sawesc || sawq) {
        syntaxerr("(badly formed word)");
    }
    return(buf);
}

static char *getword(char **s)
{
    if (**s == 0) {
        return(CNULL);
    } else {
        return getstring(s);
    }
}
