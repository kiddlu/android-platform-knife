CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

TARGET = mkfs.fat
TARGET_EXE = $(TARGET)

SRC = mkfs.fat.c
OBJ = $(SRC:.c=.o)

CFLAGS = -Wall -Wextra -Wno-sign-compare -Wno-missing-field-initializers -Wmissing-prototypes -Wstrict-prototypes -Wwrite-strings
CFLAGS += -O2 -fomit-frame-pointer -D_GNU_SOURCE $(shell getconf LFS_CFLAGS)

$(TARGET_EXE):$(OBJ);$(CC) -o $@ $^ $(LDFLAGS)
$(filter %.o,$(OBJ)):%.o:src/%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 mv $(TARGET_EXE) ../../output/bin/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_EXE)
