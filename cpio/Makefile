#makefile for mkbootfs on Windows

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld

TARGET=mkbootfs

TARGET_EXE=$(TARGET)
OBJ = mkbootfs.o
CFLAGS = -Werror
CFLAGS += -I../include

$(TARGET_EXE):$(OBJ);$(CC) -o $@ $<
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_EXE)
