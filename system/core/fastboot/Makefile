CC = $(CROSS_COMPILE)gcc

COMPILER = $(shell $(CC) -dM -E -  < /dev/null | grep _WIN32 > /dev/null && echo "mingw"  || echo "none")
ifeq ($(COMPILER), none)
COMPILER = $(shell $(CC) -dM -E -  < /dev/null | grep __CYGWIN__ > /dev/null && echo "cygwin"  || echo "none")
endif
ifeq ($(COMPILER), none)
COMPILER = linux
endif

TARGET=fastboot

TARGET_EXE=$(TARGET)

SRC = protocol.c engine.c bootimg.c fastboot.c util.c fs.c
OBJ = $(SRC:.c=.o)
DEPEND_STATIC_LIB = ../../../output/lib/libsparse.a ../../../output/lib/libz.a ../../../output/lib/libzipfile.a ../../../output/lib/libext4_utils.a

CFLAGS = -std=gnu99 -Werror
CFLAGS += -I../include -I../mkbootimg -I../libsparse/include -I../../extras/ext4_utils -I../../extras/f2fs_utils

ifeq ($(COMPILER), linux)
SRC += usb_linux.c util_linux.c
endif
ifeq ($(COMPILER), cygwin)
SRC += usb_windows.c util_windows.c
CFLAGS += -lpthread -I../../../development/host/windows/usb/api
DEPEND_STATIC_LIB += ../../../development/host/windows/prebuild/usb/AdbWinApi.a
endif
ifeq ($(COMPILER), mingw)
SRC += usb_windows.c util_windows.c
CFLAGS += -lws2_32 -I../../../development/host/windows/usb/api -DUSE_MINGW
DEPEND_STATIC_LIB += ../../../development/host/windows/prebuild/usb/AdbWinApi.a
endif

$(TARGET_EXE):$(OBJ) $(DEPEND_STATIC_LIB);$(CC) -o $@ $^
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 cp -rf $(TARGET_EXE) ../../../output/bin/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_EXE)
