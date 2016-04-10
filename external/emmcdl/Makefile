CC = $(CROSS_COMPILE)g++

COMPILER = $(shell $(CC) -dM -E -  < /dev/null | grep _WIN32 > /dev/null && echo "mingw"  || echo "none")
ifeq ($(COMPILER), none)
COMPILER = linux
endif

TARGET=emmcdl
TARGET_EXE=$(TARGET)

SRC = \
	src/crc.cpp \
	src/dload.cpp \
	src/emmcdl.cpp \
	src/ffu.cpp \
	src/firehose.cpp \
	src/partition.cpp \
	src/protocol.cpp \
	src/sahara.cpp \
	src/sparse.cpp \
	src/xmlparser.cpp

OBJ = $(SRC:.cpp=.o)
DEPEND_STATIC_LIB = 

CFLAGS = -std=c++11 -fpermissive -DUNICODE -D_UNICODE
CFLAGS += -I./inc
LDFLAGS = -static

ifeq ($(COMPILER), linux)
SRC += src/diskwriter_linux.cpp src/usbport.cpp src/usb_linux.cpp
LDFLAGS += -lpthread
#DEPEND_STATIC_LIB += ../../../output/lib/libselinux.a ../../../output/lib/libpcre.a
endif
ifeq ($(COMPILER), mingw)
#SRC += src/diskwriter_windows.cpp src/sysdeps_win32.cpp
SRC += src/serialport.cpp src/diskwriter.cpp
CFLAGS += -D_WIN32_WINNT=0x0600
#DEPEND_STATIC_LIB += ../../../prebuild/host/windows-x64/AdbWinApi.lib
#LDFLAGS += -lws2_32
endif

$(TARGET_EXE):$(OBJ) $(DEPEND_STATIC_LIB);$(CC) -o $@ $^ $(LDFLAGS)
$(filter %.o,$(OBJ)):%.o:%.cpp;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 cp -rf $(TARGET_EXE) ../../../output/bin/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_EXE) src/*.o
