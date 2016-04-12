CC = $(CROSS_COMPILE)gcc

TARGET=microcom
TARGET_EXE=$(TARGET)

SRC = microcom.c mux.c script.c help.c autodet.c
OBJ = $(SRC:.c=.o)

DEPEND_STATIC_LIB =

CFLAGS = -Wall
CFLAGS += -I.

$(TARGET_EXE):$(OBJ) $(DEPEND_STATIC_LIB);$(CC) -o $@ $^
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 mv $(TARGET_EXE) ../../output/bin/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_EXE)
