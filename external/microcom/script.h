/***************************************************************************
** File: script.h
** Description: proto-types for script.c module
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
** Rev. 1.02 - June 2000
****************************************************************************/
#ifndef SCRIPT_H  /* protection for multiple include */
#define SCRIPT_H

#define OK  0
#define ERR -1
#define RETURN  1
#define BREAK   2

typedef struct line {
    char *line;
    int labelcount;
    int lineno;
    struct line *next;
} LINE;

typedef struct var {
    char *name;
    int value;
    struct var *next;
} VAR;

#define LNULL ((LINE*)0)
#define VNULL ((VAR*)0)
#ifndef CNULL
#define CNULL ((char *)0)
#endif

/*
 * Structure describing the script we are currently executing.
 */
typedef struct {
    LINE* lines;      /* Start of all lines */
    VAR* vars;        /* Start of all variables */
    char* scriptname;     /* Name of this script */
    LINE* thisline;       /* next line to be executed */
    int in_timeout;       /* in timeout flag */
} ENV;

#endif /* SCRIPT_H */

