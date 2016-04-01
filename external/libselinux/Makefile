CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

TARGET = libselinux
TARGET_STATIC_LIB = $(TARGET).a

SRC = \
	src/callbacks.c \
	src/check_context.c \
	src/freecon.c \
	src/init.c \
	src/label.c \
	src/label_file.c \
	src/label_android_property.c

OBJ = $(SRC:.c=.o)

DEPEND_STATIC_LIB = ../../output/lib/libmincrypt.a

CFLAGS = -Wall -W -Wundef -Wshadow -Wmissing-noreturn -Wmissing-format-attribute
CFLAGS += -I./include -I../pcre

$(TARGET_STATIC_LIB):$(OBJ) $(DEPEND_STATIC_LIB);$(AR) -r $(TARGET_STATIC_LIB) $(OBJ)
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 mv $(TARGET_STATIC_LIB) ../../output/lib/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_STATIC_LIB)
