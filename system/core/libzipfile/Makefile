CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

TARGET = libzipfile
TARGET_STATIC_LIB = $(TARGET).a

SRC = centraldir.c zipfile.c
OBJ = $(SRC:.c=.o)

CFLAGS = -Werror -DHAVE_OFF64_T
CFLAGS += -I../include -I../../../external/zlib

$(TARGET_STATIC_LIB):$(OBJ);$(AR) -r $(TARGET_STATIC_LIB) $(OBJ)
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 cp -rf $(TARGET_STATIC_LIB) ../../../output/lib/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_STATIC_LIB)


