CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

TARGET = libpcre
TARGET_STATIC_LIB = $(TARGET).a

SRC = \
    pcre_chartables.c \
    dist/pcre_byte_order.c \
    dist/pcre_compile.c \
    dist/pcre_config.c \
    dist/pcre_dfa_exec.c \
    dist/pcre_exec.c \
    dist/pcre_fullinfo.c \
    dist/pcre_get.c \
    dist/pcre_globals.c \
    dist/pcre_jit_compile.c \
    dist/pcre_maketables.c \
    dist/pcre_newline.c \
    dist/pcre_ord2utf8.c \
    dist/pcre_refcount.c \
    dist/pcre_string_utils.c \
    dist/pcre_study.c \
    dist/pcre_tables.c \
    dist/pcre_ucd.c \
    dist/pcre_valid_utf8.c \
    dist/pcre_version.c \
    dist/pcre_xclass.c

OBJ = $(SRC:.c=.o)

CFLAGS = -DHAVE_CONFIG_H
CFLAGS += -I./dist -I./

$(TARGET_STATIC_LIB):$(OBJ);$(AR) -r $(TARGET_STATIC_LIB) $(OBJ)
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 mv $(TARGET_STATIC_LIB) ../../output/lib/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_STATIC_LIB)
