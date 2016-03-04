CC = $(CROSS_COMPILE)gcc

COMPILER = $(shell $(CC) -dM -E -  < /dev/null | grep _WIN32 > /dev/null && echo "mingw"  || echo "none")
ifeq ($(COMPILER), none)
COMPILER = linux
endif

TARGET=adb

TARGET_EXE=$(TARGET)

SRC =   adb.c \
        console.c \
        transport.c \
        transport_local.c \
        transport_usb.c \
        commandline.c \
        adb_client.c \
        adb_auth_host.c \
        sockets.c \
        services.c \
        file_sync_client.c \
        usb_vendors.c

OBJ = $(SRC:.c=.o)
DEPEND_STATIC_LIB = ../../../output/lib/libunz.a ../../../output/lib/libzipfile.a ../../../output/lib/libcrypto_static.a

CFLAGS = -O2 -g -DADB_HOST=1 -Wall -Wno-unused-parameter -Werror -D_XOPEN_SOURCE -D_GNU_SOURCE
CFLAGS += -I../include -I../../../external/openssl/include -I./

ifeq ($(COMPILER), linux)
SRC += usb_linux.c get_my_path_linux.c
CFLAGS += -DWORKAROUND_BUG6558362 -DHAVE_FORKEXEC
DEPEND_STATIC_LIB += ../../../output/lib/libcutils.a
LDFLAGS += -lrt -ldl -lpthread
endif
ifeq ($(COMPILER), mingw)
SRC += usb_windows.c util_windows.c
CFLAGS += -I../../../development/host/windows/usb/api -DUSE_MINGW
DEPEND_STATIC_LIB += ../../../prebuild/host/windows-x64/AdbWinApi.lib
LDFLAGS += -lws2_32
endif

$(TARGET_EXE):$(OBJ) $(DEPEND_STATIC_LIB);$(CC) -o $@ $^ $(LDFLAGS)
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 cp -rf $(TARGET_EXE) ../../../output/bin/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_EXE) *.o
