/*===========================================================================
 *  FILE:
 *  kickstart_utils.c
 *
 *  DESCRIPTION:
 *
 *  The module provides wrappers for file io operations and linked list operations
 *  for storing image id and file mapping
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
 *  kickstart_utils.c : The module provides wrappers for file io operations and linked list operations
 *  for storing image id and file mapping
 * ==========================================================================================
 *   $Header: //components/rel/boot.xf/1.0/QcomPkg/Tools/storage/fh_loader/QSaharaServer/src/kickstart_utils.c#1 $
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

/* ----------------------------------------------------------------------------- */
/* Include Files */
/* ----------------------------------------------------------------------------- */
#include "common_protocol_defs.h"
#include "kickstart_utils.h"
#include "kickstart_log.h"
#ifdef WINDOWSPC
#include "windows_utils.h"
#endif

static const char* search_paths[MAX_SEARCH_PATHS] = {0};
static int num_search_paths = 0;
static char full_filename_with_path[MAX_FILE_NAME_SIZE];

bool add_search_path(const char* path) {
	if ('/' != path[strlen(path) - 1] && '\\' != path[strlen(path) - 1]) {
		dbg (LOG_ERROR, "Search paths must terminate with a slash");
		return false;
	}

	if (MAX_SEARCH_PATHS == num_search_paths) {
		dbg (LOG_ERROR, "Cannot accept more than %d search paths", MAX_SEARCH_PATHS);
		return false;
	}
	else {
		search_paths[num_search_paths++] = path;
		return true;
	}
}

bool init_search_path_list() 
{
#ifdef WINDOWSPC
	add_search_path("C:\\builds\\Perforce_main\\core\\storage\\tools\\kickstart\\common\\kickstart\\kickstart\\");
#endif
	return add_search_path("/firmware/images/");
}

void show_search_paths(void)
{
	int i;
	printf("Search paths:\n");
	for(i = 0; i < num_search_paths; i++) {
		printf("'%s'\n", search_paths[i]);
	}
}

const char* find_file(const char *filename)
{
	int i = 0;
	struct stat status_buf;

	for (i = 0; i < num_search_paths; i++) {
		dbg(LOG_INFO, "search path %d: %s", i, search_paths[i]);
		if (strlcpy(full_filename_with_path, search_paths[i], sizeof(full_filename_with_path)) >= sizeof(full_filename_with_path)) {
			dbg(LOG_ERROR, "String was truncated while copying");
			return NULL;
		}
		if (strlcat(full_filename_with_path, filename, sizeof(full_filename_with_path)) >= sizeof(full_filename_with_path)) {
			dbg(LOG_ERROR, "String was truncated while copying");
			return NULL;
		}
		if (stat(full_filename_with_path, &status_buf) == 0) {
			dbg(LOG_INFO, "Found '%s'", full_filename_with_path);
			return full_filename_with_path;
		}
	}
	if (stat(filename, &status_buf) == 0) {
		dbg(LOG_INFO, "Found '%s' in local directory", filename);
		return filename;
	}

	dbg(LOG_ERROR, "Could not find the file '%s', returning NULL",filename);
	return NULL;
}

int open_file(const char *file_name, bool for_reading)
{
    static bool created_path_to_save_files = false;
	int fd = -1;
    char full_filename[MAX_FILENAME_LENGTH] = {0};
	int write_flags = O_CREAT | O_WRONLY | O_TRUNC;

#ifndef WINDOWSPC
#ifndef LINUXPC
	struct stat fileInfo;
#endif
#endif

    if (file_name == 0 || file_name[0] == 0) {
        dbg(LOG_ERROR, "Invalid file name");
        return fd;
    }

    if (for_reading) {
#ifdef WINDOWSPC
		_sopen_s(&fd, file_name, O_RDONLY | _O_BINARY, _SH_DENYNO, 0);
#else
        fd = open(file_name, O_RDONLY);
#endif
    }
    else {
        if (strlcpy(full_filename,
                    kickstart_options.path_to_save_files,
                    sizeof(full_filename)) >= sizeof(full_filename)
         || strlcat(full_filename,
                    kickstart_options.saved_file_prefix,
                    sizeof(full_filename)) >= sizeof(full_filename)
	     || strlcat(full_filename,
                    file_name,
                    sizeof(full_filename)) >= sizeof(full_filename)) {
            dbg(LOG_ERROR, "String was truncated while concatenating");
            return fd;
        }

		if(false == created_path_to_save_files && strlen(kickstart_options.path_to_save_files) > 0) 
		{
			if(create_path(kickstart_options.path_to_save_files) != true) 
			{
                dbg(LOG_ERROR, "Could not create directory \"%s\" to save files into", kickstart_options.path_to_save_files);
                return fd;
            }
			created_path_to_save_files = true;
        }
		dbg(LOG_EVENT,"Saving '%s'", full_filename);

#ifdef WINDOWSPC
		_sopen_s (&fd, full_filename, write_flags | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE);
#else
#ifndef LINUXPC
		if (stat(full_filename, &fileInfo) == 0 && S_ISBLK(fileInfo.st_mode)) {
			// If the file is a block device
			write_flags = write_flags | O_DIRECT;
		}
#endif
		fd = open (full_filename,
				   write_flags,
				   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
#endif
    }
    if (-1 == fd) {
        dbg(LOG_ERROR, "Unable to open input file %s.  Error %d: %s",
             full_filename,
             errno,
             strerror (errno));
    }
    return fd;
}

unsigned long get_file_size(int fd)
{
    struct stat fileInfo;
    if (fstat (fd, &fileInfo) != 0) {
        dbg(LOG_ERROR, "Unable to get file size for fd: %d.  Error: %s",
             errno,
             strerror (errno));
        return 0;
    }
    return fileInfo.st_size;
}

bool  close_file(int fd)
{
   if(close(fd))
   {
     dbg(LOG_ERROR, "Unable to close file descriptor. Error %s", strerror(errno));
     return false;
   }
   return true;
}

/*===========================================================================
 *  METHOD:
 *  create_path
 *
 *  DESCRIPTION:
 *  Creates a directory path if it does not already exist
 *
 *  PARAMETERS
 *  path         - directory path to check for
 *  mode         - mode to be used for any directory creation
 *
 *  RETURN VALUE:
 *  int          - 0 if path is successfully created
 *  ===========================================================================*/
bool create_path(const char* path) {
	struct stat status_buf;
	int retval;
	int permission_checked;
	int i;
	int path_len;
	char temp_dir[MAX_FILE_NAME_SIZE];

	dbg(LOG_INFO, "Inside create_path");

	permission_checked = 0;
	if (strlcpy(temp_dir, path, sizeof(temp_dir)) >= sizeof(temp_dir)) 
	{
		dbg(LOG_ERROR, "String truncated while copying");
		return false;
	}
	if ('\\' == temp_dir[strlen(temp_dir) - 1] || '/' == temp_dir[strlen(temp_dir) - 1]) {
		// Windows expects the name in stat() to not contain a trailing slash
		temp_dir[strlen(temp_dir) - 1] = '\0';
	}

	dbg(LOG_INFO, "temp_dir is '%s'",temp_dir);

	retval = stat(temp_dir, &status_buf);

	if (retval == 0) 
	{
		if (access(temp_dir, W_OK) == -1) 
		{
			dbg (LOG_ERROR, "Does not have write permissions for %s. Pausing...", temp_dir);
			sleep(25);
		}
		return true;
	}

    dbg (LOG_ERROR, "Path %s does not exist. Attempting to create..", temp_dir);

    path_len = strlen(path);
	dbg (LOG_ERROR, "path = '%s'", path);

    i = 0;
    for(i = 0; i < path_len; i++) 
	{
            if (i > 0 && (path[i] == '/' || i == path_len - 1)) 
			{
                //strlcpy(temp_dir, path, i+2);
                //temp_dir[i+1] = '\0';
				dbg (LOG_ERROR, "temp_dir now = '%s', null added at %d", temp_dir,i+1);

                retval = stat(temp_dir, &status_buf);
                if(retval != 0)
				{
						dbg (LOG_ERROR, "Calling mkdir(%s)", temp_dir);

#if defined(WINDOWSPC)
						retval = mkdir(temp_dir);
#else
						retval = mkdir(temp_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
                        if (retval != 0) {
                                dbg(LOG_ERROR, "Error creating directory '%s'. %s.\n", temp_dir, strerror(errno));
                                return false;
                        }
                        dbg(LOG_INFO, "Successfully created dir '%s'", temp_dir);
                }
                else {
                    if (permission_checked == 0 && access(temp_dir, W_OK) == -1) {
                        permission_checked = 1;
                        dbg(LOG_ERROR, "Does not have write permissions for %s. Pausing...", temp_dir);
                        sleep(25);
                    }
                }
            }
        }
        return true;
}

bool use_wakelock (wakelock_action action) {

	bool retval = true;

#ifndef WINDOWSPC
#ifndef LINUXPC
    static bool wakelock_works = true;
    const char wake_lock_name[] = "kickstart";
    const char wake_lock_file[] = "/sys/power/wake_lock";
    const char wake_unlock_file[] = "/sys/power/wake_unlock";
    const char *filename;
    int ret = 0;
    int wake_lock_name_length = strlen(wake_lock_name);
    int fd;

    if (false == wakelock_works)
        return false;

    if (action == WAKELOCK_ACQUIRE) {
        filename = wake_lock_file;
    }
    else {
        filename = wake_unlock_file;
    }
    fd = open(filename, O_WRONLY|O_APPEND);

    if (fd < 0) {
        dbg(LOG_ERROR, "Failed to open wake lock file %s: %s", filename, strerror(errno));
        wakelock_works = false;
        return false;
    }

    ret = write(fd, wake_lock_name, wake_lock_name_length);
    if (ret != wake_lock_name_length) {
        dbg(LOG_ERROR, "Error writing to %s: %s", filename, strerror(errno));
        retval = false;
    }
    else {
        dbg(LOG_STATUS, "Wrote to %s", filename);
        retval = true;
    }

    close(fd);
#endif
#endif
    return retval;
}

int is_regular_file(int fd)
{
    struct stat fileInfo;
    if (fstat(fd, &fileInfo) != 0) {
        dbg(LOG_ERROR, "Unable to stat file. Errno %d. Error: %s",
             errno,
             strerror (errno));
        return -1;
    }
    return S_ISREG(fileInfo.st_mode);
}

void print_buffer(byte* buffer, size_t length, int bytes_to_print) {
	size_t i = 0;
	char temp_output_buffer[LOG_BUFFER_SIZE] = { 0 };
	char output_buffer[LOG_BUFFER_SIZE] = { 0 };

    for (i = 0; i < MIN(length, (size_t) bytes_to_print); ++i) {
        snprintf(temp_output_buffer, sizeof(temp_output_buffer), "%02X ", buffer[i]);
        strlcat(output_buffer, temp_output_buffer, sizeof(output_buffer));
    }
    dbg(LOG_INFO, "First few bytes: %s", output_buffer);
    if (length > (size_t) bytes_to_print) {
        output_buffer[0] = '\0';
        for (i = 0; i < (size_t) bytes_to_print; ++i) {
            snprintf(temp_output_buffer, sizeof(temp_output_buffer), "%02X ", buffer[(length - bytes_to_print) + i]);
            strlcat(output_buffer, temp_output_buffer, sizeof(output_buffer));
        }
        dbg(LOG_INFO, "Last few bytes: %s", output_buffer);
    }
}

