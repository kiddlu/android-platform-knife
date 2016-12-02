/*===========================================================================
 *  FILE:
 *  common_protocol_defs.h
 *
 *  DESCRIPTION:
 *  Declaration of common protocol declarions and system header includes
 *
 *  Copyright (C) 2012 Qualcomm Technologies, Inc. All rights reserved.
 *                  Qualcomm Technologies Proprietary/GTDR
 *
 *  All data and information contained in or disclosed by this document is
 *  confidential and proprietary information of Qualcomm Technologies, Inc. and all
 *  rights therein are expressly reserved.  By accepting this material the
 *  recipient agrees that this material and the information contained therein
 *  is held in confidence and in trust and will not be used, copied, reproduced
 *  in whole or in part, nor its contents revealed in any manner to others
 *  without the express written permission of Qualcomm Technologies, Inc.
 *  ===========================================================================
 *
 *
 *  common_protocol_defs.h : Declaration of common protocol declarions and system header includes.
 * ==========================================================================================
 *   $Header: //components/rel/boot.xf/1.0/QcomPkg/Tools/storage/fh_loader/QSaharaServer/src/common_protocol_defs.h#1 $
 *   $DateTime: 2015/06/04 14:14:52 $
 *   $Author: pwbldsvc $
 *
 *  Edit History:
 *  YYYY-MM-DD		who		why
 *  -----------------------------------------------------------------------------
 *
 *  Copyright 2012 by Qualcomm Technologies, Inc.  All Rights Reserved.
 *
 *==========================================================================================
 */

#ifndef COMMON_PROTOCOL_DEFS
#define COMMON_PROTOCOL_DEFS

/*==========================================================================
 * Includes that are used in the protocol
 * =========================================================================*/
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef WINDOWSPC
#include <Windows.h>
#include <io.h>
#include <Share.h>
#else
#include <termios.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <malloc.h>
#include <stdbool.h>
#include <pthread.h>
#include <inttypes.h>
#endif
#if defined(LINUXPC) || defined(WINDOWSPC)
#include "external_utils.h"
#endif

#ifdef WINDOWSPC
typedef int bool;
#define false 0
#define true 1
#endif

/*endian conversion*/
#define  FLOPW(ray, val) \
    (ray)[0] = ((val) / 256); \
    (ray)[1] = ((val) & 0xFF)

/* Maximum length of a filename, in bytes */
#define MAX_FILE_NAME_SIZE 1024

typedef unsigned char byte;

typedef struct {
    char *path_to_save_files;
    char *saved_file_prefix;
    unsigned int port_connect_retries;
	int verbose;
} kickstart_options_t;

extern kickstart_options_t kickstart_options;

#ifdef WINDOWSPC
#define PORT_HANDLE HANDLE
#define INVALID_PORT_HANDLE_VALUE INVALID_HANDLE_VALUE
#define PORT_CLOSE CloseHandle
#define ALIGNED_FREE _aligned_free
#define W_OK 02
#define access(x, y) _access(x, y)
#define memalign(x, y) _aligned_malloc(y, x)
#define sleep(x) Sleep(x*1000)
#define usleep(x) Sleep(x)
#define S_ISREG(st_mode) (((st_mode) & S_IFMT) == S_IFREG)
#define snprintf _snprintf
#define PRIu64 "llu"
#define PRIX64 "llx"
#else
#define PORT_HANDLE int
#define INVALID_PORT_HANDLE_VALUE -1
#define PORT_CLOSE close
#define ALIGNED_FREE free
#endif

#endif
