/*
 * Copyright (C) 2012 Qualcomm Technologies, Inc. All rights reserved.
 *                    Qualcomm Technologies Proprietary/GTDR
 *
 * All data and information contained in or disclosed by this document is
 * confidential and proprietary information of Qualcomm Technologies, Inc. and all
 * rights therein are expressly reserved.  By accepting this material the
 * recipient agrees that this material and the information contained therein
 * is held in confidence and in trust and will not be used, copied, reproduced
 * in whole or in part, nor its contents revealed in any manner to others
 * without the express written permission of Qualcomm Technologies, Inc.
 *
 *
 *  comm.c : Handles com port connection and data transmission
 * ==========================================================================================
 *   $Header: //components/rel/boot.xf/1.0/QcomPkg/Tools/storage/fh_loader/QSaharaServer/src/comm.c#1 $
 *   $DateTime: 2015/06/04 14:14:52 $
 *   $Author: pwbldsvc $
 *
 *  Edit History:
 *  YYYY-MM-DD		who		why
 *  -----------------------------------------------------------------------------
 *  2010-09-28       ng      Added command mode support
 *
 *  Copyright 2012 by Qualcomm Technologies, Inc.  All Rights Reserved.
 *
 *==========================================================================================
 */

#include "common_protocol_defs.h"
#include "comm.h"
#include "kickstart_log.h"
#include "kickstart_utils.h"
#ifdef WINDOWSPC
#include "windows_utils.h"
#endif

com_port_t com_port = {
    "",                   // port_name
    INVALID_PORT_HANDLE_VALUE, // port_fd
    0,                    // total_bytes_sent
    0,                    // total_bytes_recd
    USING_USB,            // transport_medium
	SAHARA_PROTOCOL,      // protocol
    5,                    // rx_timeout
    5000,                 // sleep_btwn_reads_usec
    1048576,              // MAX_TO_READ
    1048576,              // MAX_TO_WRITE
	24                    // BYTES_TO_PRINT
};

void port_disconnect ()
{
    if (INVALID_PORT_HANDLE_VALUE == com_port.port_fd)
        return;

    dbg (LOG_INFO, "Disconnecting from com port");
    if (com_port.port_fd != INVALID_PORT_HANDLE_VALUE)
	{
#ifndef WINDOWSPC
	    tcflush (com_port.port_fd, TCIOFLUSH);
#endif
        PORT_CLOSE (com_port.port_fd);
        com_port.port_fd = INVALID_PORT_HANDLE_VALUE;
    }
}

bool port_connect (char *port_name)
{
#ifdef WINDOWSPC
	COMMTIMEOUTS timeouts;
#else
	struct termios tio;
    struct termios settings;
    bool using_tty_device;
    int retval;
#endif

    if (NULL == port_name || port_name[0] == '\0') {
        dbg(LOG_ERROR, "NULL port name");
        dbg(LOG_ERROR, "Should be a COM port on windows (\\.\\COMx)\n"
                       "or a device file on Linux (/dev/ttyUSB0)");
        return false;
    }
	com_port.port_name = port_name;

    /* Close any existing open port */
    if (com_port.port_fd != INVALID_PORT_HANDLE_VALUE)
        port_disconnect ();

#ifdef WINDOWSPC
	com_port.port_fd = CreateFileA(com_port.port_name,
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            0,              // FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
                            NULL);
    if (com_port.port_fd == INVALID_PORT_HANDLE_VALUE) {
        dbg(LOG_ERROR, "Failed to open com port handle");
		return false;
	}

	if (com_port.protocol == SAHARA_PROTOCOL) {
		if (com_port.rx_timeout > 0) {
			timeouts.ReadIntervalTimeout        = 10000;
			timeouts.ReadTotalTimeoutMultiplier = 10000;
			timeouts.ReadTotalTimeoutConstant   = 10000;
		}
		else {
			timeouts.ReadIntervalTimeout        = 0;
			timeouts.ReadTotalTimeoutMultiplier = 0;
			timeouts.ReadTotalTimeoutConstant   = 0;
		}
		timeouts.WriteTotalTimeoutMultiplier= 10000;
		timeouts.WriteTotalTimeoutConstant  = 10000;
	}
	else if (com_port.protocol == DLOAD_PROTOCOL) {
		timeouts.ReadIntervalTimeout        = 10;
        timeouts.ReadTotalTimeoutMultiplier = 1;
        timeouts.ReadTotalTimeoutConstant   = 1;
        timeouts.WriteTotalTimeoutMultiplier= 1;
        timeouts.WriteTotalTimeoutConstant  = 10;
	}
	if(!SetCommTimeouts(com_port.port_fd, &timeouts)) // Error setting time-outs
    {
        dbg(LOG_WARN, "Error setting com port timeouts");
    }

#else

    if (strstr(port_name, "/dev/ttyS") || strstr(port_name, "/dev/ttyHSL"))
        com_port.transport_medium = USING_UART;
    else if (strstr(port_name, "sdio"))
        com_port.transport_medium = USING_SDIO;

    using_tty_device = (strspn(port_name, "/dev/tty") == strlen("/dev/tty"));

    // Opening the com port
    // Change for Android port - Opening in O_SYNC mode since tcdrain() is not supported
    if (USING_SDIO == com_port.transport_medium)
        com_port.port_fd = open (port_name, O_RDWR | O_SYNC | O_NONBLOCK);
    else
        com_port.port_fd = open (port_name, O_RDWR | O_SYNC);

    if (INVALID_PORT_HANDLE_VALUE == com_port.port_fd)
        return false;

    if (USING_UART == com_port.transport_medium && true == using_tty_device) {
        dbg(LOG_INFO, "\nUSING UART DETECTED pPort='%s'",port_name);
        dbg(LOG_INFO, "\nConfiguring 115200, 8n1\n\n");

        memset(&tio,0,sizeof(tio));
        tio.c_iflag=0;
        tio.c_oflag=0;
        tio.c_cflag=CS8|CREAD|CLOCAL;           // 8n1, see termios.h for more information
        tio.c_lflag=0;
        tio.c_cc[VMIN]=1;
        tio.c_cc[VTIME]=5;
        cfsetospeed(&tio,B115200);            // 115200 baud
        cfsetispeed(&tio,B115200);            // 115200 baud
        tcsetattr(com_port.port_fd, TCSANOW, &tio);
    }

    // Clear any contents
    // Change for Android port - tcdrain() not supported
    // tcdrain (pComm->port_fd);

    if (using_tty_device)
    {
        // Configure termios settings

        // Get the COM port settings
        retval = tcgetattr (com_port.port_fd, &settings);
        if (-1 == retval) {
            dbg(LOG_ERROR, "termio settings could not be fetched Linux System Error:%s", strerror (errno));
            return false;
        }

        // CREAD to enable receiver and CLOCAL to say that the device is directly connected to the host
        cfmakeraw (&settings);
        settings.c_cflag |= CREAD | CLOCAL;

        // configure the new settings
        if (com_port.protocol != SAHARA_PROTOCOL)
            tcflush (com_port.port_fd, TCIOFLUSH);

        retval = tcsetattr (com_port.port_fd, TCSANOW, &settings);
        if (-1 == retval) {
            dbg(LOG_ERROR, "Device could not be configured: Linux System Errno: %s",
                 strerror (errno));
            return false;
        }
    }
#endif

	return true;
}

bool tx_data (byte *buffer, size_t bytes_to_send) {
    int temp_bytes_sent;
    size_t bytes_sent = 0;

    dbg(LOG_INFO, "Transmitting %d bytes", bytes_to_send);

	if (kickstart_options.verbose > 1) {
        print_buffer(buffer, bytes_to_send, com_port.BYTES_TO_PRINT);
	}

    while (bytes_sent < bytes_to_send) {

	#ifdef WINDOWSPC
		if(!WriteFile(com_port.port_fd, buffer + bytes_sent, MIN(bytes_to_send - bytes_sent, com_port.MAX_TO_WRITE), (LPDWORD) &temp_bytes_sent, NULL)) {
			dbg(LOG_ERROR, "Error occurred while writing to COM port");
			return false;
		}
		else {
            bytes_sent += temp_bytes_sent;
		}
	#else
		// dbg (INFO, "Calling flush before write");
		// tcflush (pComm->port_fd, TCIOFLUSH);
		temp_bytes_sent = write (com_port.port_fd, buffer + bytes_sent, MIN(bytes_to_send - bytes_sent, com_port.MAX_TO_WRITE));
		if (temp_bytes_sent < 0) {
			dbg(LOG_ERROR, "Write returned failure %d, errno %d, System error code: %s", temp_bytes_sent, errno, strerror (errno));
			return false;
		}
		else {
            bytes_sent += temp_bytes_sent;
		}
	#endif

	}

    com_port.total_bytes_sent += bytes_sent;
    dbg(LOG_INFO, "Total bytes sent so far: %d", com_port.total_bytes_sent);

    return true;
}

bool rx_data(byte *buffer, size_t bytes_to_read, size_t *bytes_read)
{
#ifdef WINDOWSPC
	DWORD retval;

	if(!ReadFile(com_port.port_fd, buffer, MIN(bytes_to_read, com_port.MAX_TO_READ), &retval, NULL)) {
        dbg(LOG_ERROR, "Error occurred while reading from COM port");
        return false;
    }
#else
    fd_set rfds;
    struct timeval tv;
    int retval;

    // Init read file descriptor
    FD_ZERO (&rfds);
    FD_SET (com_port.port_fd, &rfds);

    // time out initializtion.
    tv.tv_sec  = com_port.rx_timeout >= 0 ? com_port.rx_timeout : 0;
    tv.tv_usec = 0;

    retval = select (com_port.port_fd + 1, &rfds, NULL, NULL, ((com_port.rx_timeout >= 0) ? (&tv) : (NULL)));
    if (0 == retval) {
        dbg(LOG_ERROR, "Timeout Occured, No response or command came from the target!");
        return false;
    }
    if (retval < 0) {
        dbg(LOG_ERROR, "select returned error: %s", strerror (errno));
        return false;
    }

    retval = read (com_port.port_fd, buffer, MIN(bytes_to_read, com_port.MAX_TO_READ));
    if (0 == retval) {
        dbg(LOG_ERROR, "Zero length packet received or hardware connection went off");
        return false;
    }
    else if (retval < 0) {
        // tcflush (com_port.port_fd, TCIOFLUSH);
        if (EAGAIN == errno) {
            // Sleep before retrying in case we were using
            // non-blocking reads
            usleep(com_port.sleep_btwn_reads_usec);
        }
        else {
            dbg(LOG_ERROR, "Read/Write File descriptor returned error: %s, error code %d", strerror (errno), retval);
            return false;
        }
    }
#endif

    if (NULL != bytes_read)
        *bytes_read = retval;

    dbg(LOG_INFO, "Received %d bytes", retval);
    com_port.total_bytes_recd += retval;
    // dbg (INFO, "Total bytes received so far: %d", com_port.total_bytes_recd);

	if (kickstart_options.verbose > 1) {
        print_buffer(buffer, (size_t) retval, com_port.BYTES_TO_PRINT);
	}

    return true;
}

