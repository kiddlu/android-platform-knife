/******************************************************************
** File: microcom.c
** Description: the main file for microcom project
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
** Rev. 1.01 - March 2000
** Rev. 1.02 - June 2000
****************************************************************************/
#include "microcom.h"
#include <getopt.h>

int crnl_mapping; //0 - no mapping, 1 mapping
int script = 0; /* script active flag */
char scr_name[MAX_SCRIPT_NAME] = "script.scr"; /* default name of the script */
char log_file[PATH_MAX] = "microcom.log"; /* default name of the log file */
char device[MAX_DEVICE_NAME]; /* serial device name */
FILE* flog = NULL;   /* log file */
int  pf = 0;  /* port file descriptor */
struct termios pots; /* old port termios settings to restore */
struct termios sots; /* old stdout/in termios settings to restore */
extern unsigned int timeout; /* timeout value used when waiting for a string */
unsigned int options = 0x0; /* programs'options flags */
speed_t initial_speed = B115200;
static const speed_t invalid_speed_parameter = (speed_t)-1;

void init_comm(struct termios *pts)
{
    /* some things we want to set arbitrarily */
    pts->c_lflag &= ~ICANON;
    pts->c_lflag &= ~(ECHO | ECHOCTL | ECHONL);
    pts->c_cflag |= HUPCL;
    pts->c_cc[VMIN] = 1;
    pts->c_cc[VTIME] = 0;

    /* Standard CR/LF handling: this is a dumb terminal.
     * Do no translation:
     *  no NL -> CR/NL mapping on output, and
     *  no CR -> NL mapping on input.
     */
    pts->c_oflag &= ~ONLCR; /* set NO CR/NL mapping on output */
    crnl_mapping = 0;

    /*pts->c_oflag |= ONLCR;
    crnl_mapping = 1;*/
    pts->c_iflag &= ~ICRNL; /* set NO CR/NL mapping on input */

    /* set no flow control by default */
    pts->c_cflag &= ~CRTSCTS;
    pts->c_iflag &= ~(IXON | IXOFF | IXANY);

    /* set hardware flow control by default */
    /*pts->c_cflag |= CRTSCTS;
    pts->c_iflag &= ~(IXON | IXOFF | IXANY);*/

    /* set 115200 bps speed by default */
    cfsetospeed(pts, initial_speed);
    cfsetispeed(pts, initial_speed);
}

void init_stdin(struct termios *sts)
{
    /* again, some arbitrary things */
    sts->c_iflag &= ~BRKINT;
    sts->c_iflag |= IGNBRK;
    sts->c_lflag &= ~ISIG;
    sts->c_cc[VMIN] = 1;
    sts->c_cc[VTIME] = 0;
    sts->c_lflag &= ~ICANON;
    /* no local echo: allow the other end to do the echoing */
    sts->c_lflag &= ~(ECHO | ECHOCTL | ECHONL);
}

int open_logFile()
{
    int error = EXIT_SUCCESS;
    if (NULL == flog) {
        flog = fopen(log_file, "a");
        if (flog != (FILE *)NULL) {
            fprintf(stderr,"log enabled\n");
            DEBUG_MSG("loggin to file %s...",log_file);
        } else {
            error = error;
            ERROR_MSG("Cannot open %s error %d (%m)",log_file,error);
            /*write(STDOUT_FILENO, "Cannot open microcom.log\n", 26);*/
        }
    } else {
        /* file already opened */
        WARNING_MSG("file already opened (0x%p)",flog);
    }

    return error;
}

int close_logFile()
{
    int error = EXIT_SUCCESS;
    if (flog != 0) {
        if (fclose(flog) != 0) {
            error = errno;
            ERROR_MSG("error %d (%m) closing the log file %s",error,log_file);
        }
        flog = NULL; /* set to null anyway to be able to open a new one */
    } else {
        WARNING_MSG("there is no log file opened\n");
    }
    return error;
}


/********************************************************************
 Main functions
 ********************************************************************
 static void help_usage(int exitcode, char *error, char *addl)
      help with running the program
      - exitcode - to be returned when the program is ended
      - error - error string to be printed
      - addl - another error string to be printed
 void cleanup_termios(int signal)
      signal handler to restore terminal set befor exit
 int main(int argc, char *argv[]) -
      main program function
********************************************************************/

static const struct option longopts[] = {
    {"device", required_argument, NULL, 'D'},
    {"script", optional_argument, NULL, 'S'},
    {"log", optional_argument, NULL, 'L'},
    {"timeout", required_argument, NULL, 't'},
    {"filter", no_argument, NULL, 'F'},
    {"speed", required_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {NULL, 0, NULL, 0}
};

static void main_help(FILE *output)
{
    fprintf(output, "\nUsage: microcom [options]\n"
            " [options] include:\n"
            "\t-D<devfile>, --device=<devfile> use the specified serial port device;\n"
            "\t                                if a port is not provided, microcom\n"
            "\t                                will try to autodetect a connected serial device\n"
            "\t           example: -D /dev/ttyS3\n"
            "\t-S,--script                     run script from script.scr (default)\n"
            "\t-S<scrfile>,--script=<scrfile>  run script from scrfile\n"
            "\t-t<timeout>,--timeout=<timeout> initial timeout value in seconds\n"
            "\t-F,--filter                     enable filter \"printable only characters in logs\" \n"
            "\t-s,--speed=<initial speed>      set initial terminal speed (input and output, default is 115200 bps) \n"
            "\t-h,--help                       printf this help\n"
            "\t-v,--version                    display the program's version number\n"
            "\t-L,--log                        enable session logging in microcom.log file\n"
            "\t-L<logfile>,--log=<logfile>     enable session logging in the specified filename\n"
            "\n");
}

void main_usage(int exitcode, char *str, char *dev)
{
    main_help(stderr);
    fprintf(stderr, "Exitcode %d - %s %s\n\n", exitcode, str, dev);
    exit(exitcode);
}

static inline void main_version(void)
{
    printf("microcom v"VERSION"\n");
    exit(0);
}

#define S(x)    X(x,B##x,TO_STRING(x))
#define VALID_TERMINAL_SPEED_TABLE \
        S(0) \
        S(50) \
        S(75) \
        S(110) \
        S(134) \
        S(150) \
        S(200) \
        S(300) \
        S(600) \
        S(1200) \
        S(1800) \
        S(2400) \
        S(4800) \
        S(9600) \
        S(19200) \
        S(38400) \
        S(57600) \
        S(115200) \
        S(230400)

static inline speed_t check_speed_parameter(const char* value)
{
    speed_t speed = invalid_speed_parameter;
    const unsigned long int requested_speed = strtoul(value, NULL,0);
    switch(requested_speed) {
#define X(x,y,z) case x: return y;
        VALID_TERMINAL_SPEED_TABLE
    default:
        return invalid_speed_parameter;
#undef X
    }
}

static inline const char* valid_terminal_speeds()
{
#define X(x,y,z) "\n\t -" z
    return VALID_TERMINAL_SPEED_TABLE;
#undef X
}

#undef VALID_TERMINAL_SPEED_TABLE
#undef S

static inline int parse_cmdLine(int argc, char *argv[])
{
    int error = EXIT_SUCCESS;
    int optc;

    while (((optc = getopt_long (argc, argv, "D:S::L::t:Fs:hv", longopts, NULL)) != -1) && (EXIT_SUCCESS == error)) {
        int param;
        switch (optc) {
        case 'D':
            strncpy(device, optarg, MAX_DEVICE_NAME);
            break;
        case 'S':
            script = 1; /* set script flag */
            if (optarg != NULL) {
                strncpy(scr_name, optarg, MAX_SCRIPT_NAME);
                DEBUG_VAR(scr_name,"%s");
            }
            break;
        case 'L':
            if (optarg != NULL) {
                strncpy(log_file,optarg,PATH_MAX);
                DEBUG_VAR(log_file,"%s");
            }
            open_logFile();
            break;
        case 't': {
            const int value = atoi(optarg);
            if (value >= 1) {
                timeout = value;
            } else {
                fprintf(stderr,"invalid argument (%d) to set initial time out value\n",value);
                error = EINVAL;
            }
            DEBUG_VAR(timeout,"%d");
        }
        break;
        case 'F':
            options |= OPTION_LOG_FILTER;
            break;
        case 's': {
            const speed_t requested_speed = check_speed_parameter(optarg);
            if (requested_speed != invalid_speed_parameter) {
                initial_speed = requested_speed;
            } else {
                fprintf(stderr,"invalid speed parameters arguments %s, valid values are %s\n",optarg,valid_terminal_speeds());
            }
        }
        break;
        case 'h':
            main_help(stdout);
            exit(error);
            break;
        case 'v':
            main_version();
            break;
        case '?':
            error = EINVAL;
            exit(error); /* because of current design */
            break;
        default:
            error = EINVAL;
            fprintf(stderr,"invalid argument (%d)\n",optc);
            main_help(stderr);
            exit(error); /* because of current design */
            break;
        } /* switch (optc) */

    } /* while (((optc = getopt_long (argc, argv, "D:S:Lt:fhv", longopts, NULL)) != -1) && (EXIT_SUCCESS == error)) */
    return error;
}

void exit_program(int exitCode)
{
    /* close the log file first */
    if (flog) {
        close_logFile();
    }
    tcsetattr(pf, TCSANOW, &pots);
    tcsetattr(STDIN_FILENO, TCSANOW, &sots);
    exit(exitCode);
}

/* restore original terminal settings on exit */
void cleanup_termios(int signal)
{
    if ((flog != NULL) && (signal != 0)) {
        fprintf(flog,"exiting on signal %d\n",signal);
    }
    exit_program(0);
}

int main(int argc, char *argv[])
{
    int error = EXIT_SUCCESS;
    struct  termios pts;  /* termios settings on port */
    struct  termios sts;  /* termios settings on stdout/in */
    struct sigaction sact;/* used to initialize the signal handler */

    device[0] = '\0';

    /* parse command line */
    error = parse_cmdLine(argc,argv);

    if ('\0' == device[0]) {
        /* try to find a connected serial device */
        pf = autodetect();
        if (pf < 0) {
            main_usage(2, "cannot found a connected serial device", device);
        }
    } else {
        /* open the device */
        pf = open(device, O_RDWR);
        if (pf < 0) {
            const int error = errno;
            ERROR_MSG("cannot open device %s, error = %d (%m)", device,error);
            main_usage(2, "cannot open device", device);
        }
    }

    /* modify the port configuration */
    tcgetattr(pf, &pts);
    memcpy(&pots, &pts, sizeof(pots));
    init_comm(&pts);
    tcsetattr(pf, TCSANOW, &pts);

    /* Now deal with the local terminal side */
    tcgetattr(STDIN_FILENO, &sts);
    memcpy(&sots, &sts, sizeof(sots)); /* to be used upon exit */
    init_stdin(&sts);
    tcsetattr(STDIN_FILENO, TCSANOW, &sts);

    /* set the signal handler to restore the old
     * termios handler */
    sact.sa_handler = cleanup_termios;
    sigaction(SIGHUP, &sact, NULL);
    sigaction(SIGINT, &sact, NULL);
    sigaction(SIGPIPE, &sact, NULL);
    sigaction(SIGTERM, &sact, NULL);

    /* run the main program loop */
    mux_loop(pf);

    /* restore original terminal settings and exit */
    tcsetattr(pf, TCSANOW, &pots);
    tcsetattr(STDIN_FILENO, TCSANOW, &sots);

    exit(error);
}
