CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

TARGET=libz
OBJ = adler32.o compress.o crc32.o deflate.o gzclose.o gzlib.o gzread.o gzwrite.o infback.o inflate.o inftrees.o inffast.o trees.o uncompr.o zutil.o
CFLAGS = -Wall -Werror
TARGET_STATIC_LIB = $(TARGET).a
CFLAGS += -I../include -O3 -DUSE_MMAP

$(TARGET_STATIC_LIB):$(OBJ);$(AR) -r $(TARGET_STATIC_LIB) $(OBJ)
$(filter %.o,$(OBJ)):%.o:src/%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 mv $(TARGET_STATIC_LIB) ../output/lib/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_STATIC_LIB)
