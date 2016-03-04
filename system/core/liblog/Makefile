CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

TARGET=liblog
TARGET_STATIC_LIB = $(TARGET).a

SRC = logd_write.c logprint.c event_tag_map.c fake_log_device.c
OBJ = $(SRC:.c=.o)

CFLAGS = -Werror -DFAKE_LOG_DEVICE=1
CFLAGS += -I../include

$(TARGET_STATIC_LIB):$(OBJ) $(DEPEND_STATIC_LIB);$(AR) -r $(TARGET_STATIC_LIB) $(OBJ)
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 cp -rf $(TARGET_STATIC_LIB) ../../../output/lib/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_STATIC_LIB)
