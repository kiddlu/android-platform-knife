#/******************************************************************
#** File: Makefile
#** Description: the makefile for microcom project
#**
#** Copyright (C)1999 Anca and Lucian Jurubita <ljurubita@hotmail.com>.
#** All rights reserved.
#****************************************************************************
#** This program is free software; you can redistribute it and/or
#** modify it under the terms of the GNU General Public License
#** as published by the Free Software Foundation; either version 2
#** of the License, or (at your option) any later version.
#**
#** This program is distributed in the hope that it will be useful,
#** but WITHOUT ANY WARRANTY; without even the implied warranty of
#** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#** GNU General Public License for more details at www.gnu.org
#****************************************************************************
#** Rev. 0.9 - Sept. 1999
#** Rev. 0.91 - Jan. 2000 - minor fixes, compiled under Mandrake 6.0
#****************************************************************************/
CFLAGS = -O0 -ggdb3 -D_DEBUG_
LDFLAGS = -O0 -ggdb3
CC = gcc
 
microcom: microcom.o mux.o script.o help.o autodet.o
	$(CC) $(LDFLAGS) -o microcom microcom.o mux.o script.o help.o autodet.o

autodet.o: autodet.c microcom.h
	$(CC) $(CFLAGS) -c autodet.c

script.o: script.c script.h microcom.h
	$(CC) $(CFLAGS) -c script.c

mux.o: mux.c microcom.h
	$(CC) $(CFLAGS) -c mux.c

microcom.o: microcom.c microcom.h
	$(CC) $(CFLAGS) -c microcom.c

help.o: help.c microcom.h
	$(CC) $(CFLAGS) -c help.c

clean:
	/bin/rm -f microcom microcom.o mux.o script.o help.o autodet.o
