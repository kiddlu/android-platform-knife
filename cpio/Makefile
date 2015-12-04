#makefile for mkbootfs on Windows

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
EXEC = mkbootfs
OBJ = mkbootfs.o
SRC = mkbootfs.c
CFLAGS += -I../include
$(EXEC):$(OBJ);$(CC) -o $@ $<
$(OBJ):$(SRC);$(CC) $(CFLAGS) -c -o $@ $<

clean:
	 rm -f $(OBJ) $(EXEC)
