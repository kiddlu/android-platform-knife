CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

COMPILER = $(shell $(CC) -dM -E -  < /dev/null | grep _WIN32 > /dev/null && echo "mingw"  || echo "none")
ifeq ($(COMPILER), none)
COMPILER = linux
endif

TARGET=liblog
TARGET_STATIC_LIB = $(TARGET).a

SRC = logd_write.c fake_log_device.c
OBJ = $(SRC:.c=.o)

CFLAGS = -Werror -DFAKE_LOG_DEVICE=1
CFLAGS += -I../include

ifeq ($(COMPILER), linux)
SRC += logprint.c event_tag_map.c
CFLAGS += -DHAVE_SYS_UIO_H
endif
ifeq ($(COMPILER), mingw)
SRC += uio.c
CFLAGS += -DUSE_MINGW
endif

$(TARGET_STATIC_LIB):$(OBJ) $(DEPEND_STATIC_LIB);$(AR) -r $(TARGET_STATIC_LIB) $(OBJ)
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 cp -rf $(TARGET_STATIC_LIB) ../../../output/lib/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_STATIC_LIB)
