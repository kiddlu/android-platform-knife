/*===========================================================================
 *  FILE:
 *  kickstart_utils.h
 *
 *  DESCRIPTION:
 *  File io wrapper functions and <id, file_name> linked list interfaces
 *
 *  Copyright (C) 2012 Qualcomm Technologies Incorporated. All rights reserved.
 *                  QUALCOMM Proprietary/GTDR
 *
 *  All data and information contained in or disclosed by this document is
 *  confidential and proprietary information of Qualcomm Technologies Incorporated and all
 *  rights therein are expressly reserved.  By accepting this material the
 *  recipient agrees that this material and the information contained therein
 *  is held in confidence and in trust and will not be used, copied, reproduced
 *  in whole or in part, nor its contents revealed in any manner to others
 *  without the express written permission of Qualcomm Technologies Incorporated.
 *  ===========================================================================
 *
 *
 *  kickstart_utils.h : File io wrapper functions and <id, file_name> linked list interfaces
 * ==========================================================================================
 *   $Header: //components/rel/boot.xf/1.0/QcomPkg/Tools/storage/fh_loader/QSaharaServer/src/kickstart_utils.h#1 $
 *   $DateTime: 2015/06/04 14:14:52 $
 *   $Author: pwbldsvc $
 *
 *  Edit History:
 *  YYYY-MM-DD		who		why
 *  -----------------------------------------------------------------------------
 *  2013-01-10      ah       filenames are no longer in linked list
 *
 *  Copyright 2012 by Qualcomm Technologies, Incorporated.  All Rights Reserved.
 *
 *==========================================================================================
 */
#ifndef KICKSTART_UTILS_H
#define KICKSTART_UTILS_H

#include "common_protocol_defs.h"

#define MAX_FILENAME_LENGTH 1024
#define MAX_SEARCH_PATHS 8

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

void show_search_paths();
bool add_search_path(const char *path);
bool init_search_path_list(void);
const char* find_file(const char *filename);

int open_file(const char *file_name, bool for_reading);
unsigned long get_file_size(int fd);
bool  close_file(int fp);

typedef enum {
    WAKELOCK_RELEASE,
    WAKELOCK_ACQUIRE
} wakelock_action;
bool use_wakelock (wakelock_action action);

bool create_path(const char* path);
int is_regular_file(int fd);

void print_buffer(byte* buffer, size_t length, int bytes_to_print);

#endif
