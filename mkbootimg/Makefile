#makefile for mkbootfs on Windows

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
EXEC = mkbootimg
OBJ = mkbootimg.o
STATIC_LIB = ../libmincrypt/libmincrypt.a
CFLAGS += -I../include

$(EXEC):$(OBJ);$(CC) -o $@ $< $(STATIC_LIB)
%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

clean:
	 rm -f $(OBJ) $(EXEC)
