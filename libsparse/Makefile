CC = $(CROSS_COMPILE)gcc

TARGET=simg2img

TARGET_EXE=$(TARGET)
OBJ = simg2img.o sparse_crc32.o backed_block.o output_file.o sparse.o sparse_err.o sparse_read.o
DEPEND_STATIC_LIB = ../output/lib/libz.a
CFLAGS = -Wall
CFLAGS += -I../include -I./include -I../zlib/src

$(TARGET_EXE):$(OBJ) $(DEPEND_STATIC_LIB);$(CC) -o $@ $^
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 mv $(TARGET_EXE) ../output/bin/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_EXE)
