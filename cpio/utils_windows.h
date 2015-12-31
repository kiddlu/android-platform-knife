/*
 * utils_windows.h
 * 
 * Copyright 2013 Trevor Drake <trevd1234@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#ifndef _a837e23a_9629_11e2_9d73_5404a601fa9d
#define _a837e23a_9629_11e2_9d73_5404a601fa9d
#include <sys/stat.h>
#include <windows.h>

#define S_IFLNK  0120000
#define S_IFSOCK 0140000
#define S_IWGRP 00020
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)

#ifndef WINDOWS_EOL
#define WINDOWS_EOL "\r\n"
#endif

// Handle win64 because they like to make shit up
#ifdef __WIN64__
typedef	unsigned long long	ulong_t;
typedef	long long	long_t;
#else
typedef	unsigned long	ulong_t;
typedef	long long_t;
#endif

#define CONVERT_LINE_ENDINGS 1==1
#define EOL WINDOWS_EOL

#define BLKGETSIZE64 0

#define ENODATA 61

// redefine mkdir on windows
#define __mkdir(path,mode) mkdir(path)

// 
int lstat(const char *path, struct stat *buf);
ssize_t readlink(const char *path, char *buf, size_t bufsiz);
int symlink(const char *source, const char *path);
#endif
