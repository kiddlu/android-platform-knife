/******************************************************************
** File: help.c
** Description: implements the help screens
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
** Rev. 1.02 - June 2000
****************************************************************************/
#include "microcom.h"

#define MICROCOM_PROMPT "Microcom cmd:" /*Command:*/ /* to avoid confusion with the program output received by the serial link */

extern int crnl_mapping; //0 - no mapping, 1 mapping
extern int script; /* script active flag */
extern char scr_name[MAX_SCRIPT_NAME]; /* name of the script */
extern char device[MAX_DEVICE_NAME]; /* serial device name */
//extern int logFlag; /* log active flag */
extern FILE* flog;   /* log file */
extern char log_file[PATH_MAX]; /* name of the log file */

static int help_state = 0;
static int in_escape = 0;

/* static functions proto-types */
static void help_usage(int exitcode, char *error, char *addl);
static void help_escape(void);
static void help_terminal(void);
static void help_speed(void);
static void help_send_escape(int fd, char c);
static void help_done(void);

/******************************************************************
 Help handling functions
*******************************************************************
 static void help_escape(void) - print the main help screen
 static void help_terminal(void)  - print terminal help screen
 static void help_speed(void) - print the speed help screen
 static void help_send_escape(int fd, char c) -
        handle the main help sereen
        - fd - file handle for comm port
        - c - carachter to be processed; if it is not recognized
              as a valid help character, it will be sent to the
              com port (fd)
 static void help_set_terminal(int fd, char c) -
        handle terminal help screen
        - same params as for help_send_escape()
 static void help_set_speed(int fd, char c)
        handle speed help screen
        - same params as for help_send_escape()
 void cook_buf(int fd, char *buf, int num) -
        redirect escaped characters to the help handling functions;
    the function is called from mux.c in the main character
        processing ruoutine;
        - fd - file handle for the comm port
        - buf - pointer to the character buffer
        - num - number of valid characters in the buffer
******************************************************************/
static void help_done(void)
{
    char done_str[] = "Help done!\n";
    write(STDOUT_FILENO, done_str, strlen(done_str));
}



static void help_escape(void)
{
    char str1[] =
        "\n"
        "**********Help***********\n"
        "  x - exit microcom\n"
        "  b - send break\n";

    char str2[] =
        "  t - set terminal\n"
        "  q - quit help\n"
        "*************************\n"
        MICROCOM_PROMPT;

    write(STDOUT_FILENO, str1, strlen(str1));

    if (flog == 0) {
        write(STDOUT_FILENO, "  l - log on             \n", 26);
    } else {
        write(STDOUT_FILENO, "  l - log off            \n", 26);
    }

    if (script == 1) {
        write(STDOUT_FILENO, "  s - stop script        \n", 26);
    } else {
        write(STDOUT_FILENO, "  s - start script       \n", 26);
    }

    write(STDOUT_FILENO, str2, strlen(str2));
}

static void help_terminal(void)
{
    char str1[] =
        "\n"
        "******Set terminal ******\n"
        "  p - set speed\n";

    char str2[] =
        "  n - no flow control\n"
        "  h - hardware flow control\n"
        "  s - software flow control\n"
        "  q - quit help\n"
        "*************************\n"
        MICROCOM_PROMPT;

    write(STDOUT_FILENO, str1, strlen(str1));
    if (crnl_mapping) {
        write(STDOUT_FILENO, "  m - no CR/NL mapping   \n", 26);
    } else {
        write(STDOUT_FILENO, "  m - NL to CR/NL mapping\n", 26);
    }
    write(STDOUT_FILENO, str2, strlen(str2));
}

static void help_speed(void)
{
    char str[] =
        "\n******Set speed *********\n"
        " a - 1200\n"
        " b - 2400\n"
        " c - 4800\n"
        " d - 9600\n"
        " e - 19200\n"
        " f - 38400\n"
        " g - 57600\n"
        " h - 115200\n"
        " i - 230400\n"
        " j - 460800\n"
        " q - quit help\n"
        "*************************\n"
        MICROCOM_PROMPT;

    write(STDOUT_FILENO, str, strlen(str));
}

/* process function for help_state=0 */
static void help_send_escape(int fd, char c)
{
    struct  termios pts;  /* termios settings on port */

    in_escape = 0;
    help_state = 0;

    switch (c) {
    case 'x':
        /* restore termios settings and exit */
        write(STDOUT_FILENO, "\n", 1);
        cleanup_termios(0);
        break;
    case 'q': /* quit help */
        break;
    case 'l': /* log on/off */
        if (flog == 0) { /* open log file */
            open_logFile(log_file);
        } else { /* close log file */
            close_logFile();
        }
        break;
    case 's': /* script active/inactive */
        script_init(scr_name);
        script = (script)? 0: 1;
        break;
    case 'b': /* send break */
        /* send a break */
        tcsendbreak(fd, 0);
        break;
    case 't': /* set terminal */
        help_state = 1;
        help_terminal();
        in_escape = 1;
        break;
    case '~': /* display it again */
        help_escape();
        break;
    default:
        /* pass the character through */
        /* "C-\ C-\" sends "C-\" */
        write(fd, &c, 1);
        break;
    }

    if (in_escape == 0)
        help_done();

    return;
}

/* process function for help_state=1 */
static void help_set_terminal(int fd, char c)
{
    struct  termios pts;  /* termios settings on port */
    tcgetattr(fd, &pts);
    switch (c) {
    case 'm': /* CR/NL mapping */
        in_escape = 0; /* get it out from escape state */
        help_state = 0;
        if (crnl_mapping) {
            pts.c_oflag &= ~ONLCR;
            crnl_mapping = 0;
        } else {
            pts.c_oflag |= ONLCR;
            crnl_mapping = 1;
        }
        DEBUG_MSG("Map CarriageReturn to NewLine on output = %d",crnl_mapping);
        tcsetattr(fd, TCSANOW, &pts);
        break;
    case 'p': /* port speed */
        in_escape = 1;
        help_state = 2;
        help_speed();
        break;
    case 'h': /* hardware flow control */
        in_escape = 0; /* get it out from escape state */
        help_state = 0;
        /* hardware flow control */
        pts.c_cflag |= CRTSCTS;
        pts.c_iflag &= ~(IXON | IXOFF | IXANY);
        tcsetattr(fd, TCSANOW, &pts);
        break;
    case 's': /* software flow contrlol */
        in_escape = 0; /* get it out from escape state */
        help_state = 0;
        /* software flow control */
        pts.c_cflag &= ~CRTSCTS;
        pts.c_iflag |= IXON | IXOFF | IXANY;
        tcsetattr(fd, TCSANOW, &pts);
        break;
    case 'n': /* no flow control */
        in_escape = 0; /* get it out from escape state */
        help_state = 0;
        /* no flow control */
        pts.c_cflag &= ~CRTSCTS;
        pts.c_iflag &= ~(IXON | IXOFF | IXANY);
        tcsetattr(fd, TCSANOW, &pts);
        break;
    case '~':
    case 'q':
        in_escape = 0;
        help_state = 0;
        break;
    default:
        /* pass the character through */
        /* "C-\ C-\" sends "C-\" */
        write(fd, &c, 1);
        break;
    }

    if (in_escape == 0)
        help_done();

    return;
}

/* handle a single escape character */
static void help_set_speed(int fd, char c)
{
    struct  termios pts;  /* termios settings on port */
    speed_t speed[] = {
        B1200,
        B2400,
        B4800,
        B9600,
        B19200,
        B38400,
        B57600,
        B115200,
        B230400,
        B460800
    };

    if (c < 'a' && c > 'j') {
        if (c == '~') {
            help_speed();
            return;
        } else if (c == 'q') {
            in_escape = 0;
            help_state = 0;
        }
        write(fd, &c, 1);
        return;
    }

    tcgetattr(fd, &pts);
    cfsetospeed(&pts, speed[c - 'a']);
    cfsetispeed(&pts, speed[c - 'a']);
    tcsetattr(fd, TCSANOW, &pts);
    in_escape = 0;
    help_state = 0;
    help_done();
}


/* handle escape characters, writing to output */
void cook_buf(int fd, char *buf, int num)
{
    int current = 0;

    if (in_escape) {
        /* cook_buf last called with an incomplete escape sequence */
        switch (help_state) {
        case 0:
            help_send_escape(fd, buf[0]);
            break;
        case 1:
            help_set_terminal(fd, buf[0]);
            break;
        default:
            help_set_speed(fd, buf[0]);
        }
        num--; /* advance to the next character in the buffer */
        buf++;
    }

    while (current < num) { /* big while loop, to process all the charactes in buffer */

        /* look for the next escape character '~' */
        while ((current < num) && (buf[current] != 0x7e)) current++;
        /* and write the sequence before esc char to the comm port */
        if (current) {
            DEBUG_DUMP_MEMORY(buf,current);
            const ssize_t written = write (fd, buf, current);
            if (written == -1) {
                const int error = errno;
                ERROR_MSG("error writting to device %d (%m)",error);
            } else if (written != current) {
                ERROR_MSG("error writting to device only %d bytes written",written);
            }
        }

        if (current < num) { /* process an escape sequence */
            /* found an escape character */
            if (help_state == 0) {
                help_escape();
            }
            current++;
            if (current >= num) {
                /* interpret first character of next sequence */
                in_escape = 1;
                return;
            }

            switch (help_state) {
            case 0:
                help_send_escape(fd, buf[current]);
                break;
            case 1:
                help_set_terminal(fd, buf[current]);
                break;
            default:
                help_set_speed(fd, buf[current]);
            } /* end switch */
            current++;
            if (current >= num)
                return;
        } /* if - end of processing escape sequence */
        num -= current;
        buf += current;
        current = 0;
    } /* while - end of processing all the charactes in the buffer */
    return;
}
