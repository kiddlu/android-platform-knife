CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar

TARGET=mkbootfs

TARGET_EXE=$(TARGET)
OBJ = mkbootfs.o
CFLAGS = -Werror
CFLAGS += -I../include

$(TARGET_EXE):$(OBJ);$(CC) -o $@ $<
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 mv $(TARGET_EXE) ../output/bin/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_EXE)
