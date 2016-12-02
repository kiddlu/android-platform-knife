CC = $(CROSS_COMPILE)gcc

TARGET=fh_loader
TARGET_EXE=$(TARGET)

SRC = fh_loader_sha.c fh_loader.c
OBJ = $(SRC:.c=.o)

CFLAGS = -Wall
LDFLAGS += -lrt

$(TARGET_EXE):$(OBJ);$(CC) -o $@ $^ $(LDFLAGS)
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_EXE)
