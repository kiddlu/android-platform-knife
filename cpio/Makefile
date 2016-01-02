CC = $(CROSS_COMPILE)gcc
TARGET=mkbootfs

TARGET_EXE=$(TARGET)
OBJ = mkbootfs.o utils_windows.o
CFLAGS += -I../include

$(TARGET_EXE):$(OBJ);$(CC) -o $@ $^
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $^

.PHONY:install
install:
	mv $(TARGET_EXE) ../output/bin/
	cp ./unpackramdisk.sh ../output/bin/

.PHONY:clean
clean:
	rm -f $(OBJ) $(TARGET_EXE)
