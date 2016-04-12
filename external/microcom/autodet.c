/******************************************************************
 ** File: autodet.c
 ** Description: implements modem autodetect facility
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

extern char device[MAX_DEVICE_NAME];

#ifdef MODEM_CMD
#define LOCAL_BUF 25
static inline int autodetectModem(char *deviceName)
{
    int pf; /* port handler */
    int     i = 0;        /* used in the multiplex loop */
    int retval = 0;

    fd_set  ready;        /* used for select */
    struct timeval tv;

    char    buf[LOCAL_BUF + 1]; /* we'll add a '\0' */
    struct  termios pts;  /* new comm port settings */
    struct termios pots; /*old comm port settings */
    struct  termios sts;  /* new stdin settings */
    struct termios sots; /* old stdin settings */

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    INFO_MSG("Try %s", deviceName);

    pf = open(deviceName, O_RDWR);
    if (pf < 0) {
        const int error = errno;
        INFO_MSG("open %s error %d (%m)", deviceName,error);
        return 0;
    }

    /* modify comm port configuration */
    tcgetattr(pf, &pts);
    memcpy(&pots, &pts, sizeof(pots)); /* to be used upon exit */
    init_comm(&pts);
    tcsetattr(pf, TCSANOW, &pts);

    /* modify stdin configuration */
    tcgetattr(STDIN_FILENO, &sts);
    memcpy(&sots, &sts, sizeof(sots)); /* to be used upon exit */
    init_stdin(&sts);
    tcsetattr(STDIN_FILENO, TCSANOW, &sts);

    /* send an at&f and wait for an OK */
    FD_ZERO(&ready);
    FD_SET(pf, &ready);
    write(pf, "at&f\n", 5);
    select(pf+1, &ready, NULL, NULL, &tv);
    if (FD_ISSET(pf, &ready)) {
        sleep(2);
        /* pf has characters for us */
        i = read(pf, buf, LOCAL_BUF);
    }

    if (i > 0) {
        buf[i + 1] = '\0';
        printf(buf);
        if (strstr(buf, "OK") != NULL) {
            NOTICE_MSG("Modem found on %s", deviceName);
            retval = 1;
        }
    } else {
        NOTICE_MSG("%s not responding\n\n", deviceName);
    }

    tcsetattr(pf, TCSANOW, &pots);
    tcsetattr(STDIN_FILENO, TCSANOW, &sots);

    if (retval != 1) {
        close(pf);
        pf = -1;
    }
    return pf;
}

int autodetect(void)
{
    int i;
    int pf = -1;
    unsigned int count = 0;
    do {
        /* if no device was passed on the command line,
           try to autodetect a modem */
        for (i = 0; i < 4; i++) {
            sprintf(device, "/dev/ttyS%1d", i);
            pf = autodetectModem(device);
            if (pf != -1) {
                break;
            }
        }
        /* try again, to find FTDI USB / serial devices (if any)*/
        if (i == 4) {
            for (i = 0; i < 4; i++) {
                sprintf(device, "/dev/ttyUSB%1d", i);
                pf = autodetectModem(device);
                if (pf != -1) {
                    break;
                } /* (autodetect(i)) */
            } /* for (i = 0; i < 4; i++) */
        } /* (i == 4) */

        /* no device found, wait 1/2s before trying againt (max 10 tries) */
        if (i == 4) {
            NOTICE_MSG("no device found, waiting 1/2s before trying again...");
            usleep(500000);
            count++;
        }
    } while ((i == 4) && (count < 1200));

    if (i == 4) {
        main_usage(1, "no device found", "");
        pf = -1;
    }
    return pf;
}

#else /* MODEM_CMD */

#ifndef MAX
#define MAX(x,y) (((x)>=(y)) ? (x) : (y))
#endif

int autodetect(void)
{
    int i;
    int pf = -1;
    fd_set  serialfds;
    int serialfd[8];
    int maxfd = -1;
    unsigned int nbfds = 0;
    struct timeval timeouttv;
    int retval = 0;

    FD_ZERO(&serialfds);

    /* ttySx */
    for (i = 0; i < 4; i++) {
        sprintf(device, "/dev/ttyS%1d", i);
        serialfd[nbfds] = open(device,O_RDWR);
        if (serialfd[nbfds] != -1) {
            FD_SET(serialfd[nbfds], &serialfds);
            maxfd = MAX(maxfd,serialfd[nbfds]);
            DEBUG_MSG("device %s opened (%d)",device,nbfds);
            nbfds++;
        } else {
            const int error = errno;
            INFO_MSG("open %s error %d (%m)", device,error);
        }
    } /* for (i = 0; i < 4; i++) */

    /* ttyUSBx */
    for (i = 0; i < 4; i++) {
        sprintf(device, "/dev/ttyUSB%1d", i);
        serialfd[nbfds] = open(device,O_RDWR);
        if (serialfd[nbfds] != -1) {
            FD_SET(serialfd[nbfds], &serialfds);
            maxfd = MAX(maxfd,serialfd[nbfds]);
            DEBUG_MSG("device %s opened (%d)",device,nbfds);
            nbfds++;
        } else {
            const int error = errno;
            INFO_MSG("open %s error %d (%m)", device,error);
        }
    } /* for (i = 0; i < 4; i++) */

    if (nbfds) {
        /* the serial device selected will be the first that will send data, during the next 5 minutes starting from NOW */
        timeouttv.tv_sec = 5*60;
        timeouttv.tv_usec = 0;
        DEBUG_MSG("waiting for a character from one of the %d fd",nbfds);
        retval = select(++maxfd, &serialfds, NULL, NULL, &timeouttv); /* cf. man 2 select_tut */
        DEBUG_VAR(retval,"%d");
        if (retval != -1) {
            if (retval != 0) {
                /* close all the file descriptors except the first active one */
                for(i=0; i<nbfds; i++) {
                    if ((FD_ISSET(serialfd[i], &serialfds)) && (-1 == pf)) {
                        pf = serialfd[i];
                        NOTICE_MSG("some data are available on %d (device number %d)",pf,i);
                    } else {
                        close(serialfd[i]);
                        serialfd[i] = -1;
                    }
                }
            } else {
                ERROR_MSG("timeout (%u sec) waiting for data from a serial device",timeouttv.tv_sec);
                for(i=0; i<nbfds; close(serialfd[i++]));
            }
        } else {
            const int error = errno;
            ERROR_MSG("select on opened serial port error %d (%m)", error);
            for(i=0; i<nbfds; close(serialfd[i++]));
        }
    } else {
        ERROR_MSG("unable to open at least a /dev/ttySx or /dev/ttyUSBx device !");
        device[0] = '\0';
    }
    return pf;
}
#endif /* MODEM_CMD */
