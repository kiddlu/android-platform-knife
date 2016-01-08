CC = $(CROSS_COMPILE)gcc

TARGET=fastboot

TARGET_EXE=$(TARGET)
OBJ = usb_linux.o util_linux.o protocol.o engine.o bootimg.o fastboot.o util.o fs.o
DEPEND_STATIC_LIB = ../output/lib/libsparse.a ../output/lib/libz.a ../output/lib/libzipfile.a ../output/lib/libext4_utils.a
CFLAGS = -std=gnu99 -Werror
CFLAGS += -I../include -I../ext4_utils -I../f2fs_utils -I../mkbootimg

$(TARGET_EXE):$(OBJ) $(DEPEND_STATIC_LIB);$(CC) -o $@ $^
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 cp -rf $(TARGET_EXE) ../output/bin/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_EXE)
