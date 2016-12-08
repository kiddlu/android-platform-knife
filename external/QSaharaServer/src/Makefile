CC = $(CROSS_COMPILE)gcc
COMPILER = $(shell $(CC) -dM -E -  < /dev/null | grep _WIN32 > /dev/null && echo "mingw"  || echo "none")
ifeq ($(COMPILER), none)
COMPILER = linux
endif

TARGET=Qsahara
TARGET_EXE=$(TARGET)

SRC = kickstart.c comm.c sahara_protocol.c kickstart_log.c kickstart_utils.c external_utils.c
OBJ = $(SRC:.c=.o)
CFLAGS = -Wall -Werror

ifeq ($(COMPILER), linux)
CFLAGS += -DLINUXPC
endif
ifeq ($(COMPILER), mingw)
CFLAGS += -DWINDOWSPC
endif

CFLAGS += -I../include

$(TARGET_EXE):$(OBJ) $(DEPEND_STATIC_LIB);$(CC) -o $@ $^
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 cp -rf $(TARGET_EXE) ../../../output/bin/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_EXE)