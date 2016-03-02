CC = $(CROSS_COMPILE)gcc

TARGET=showmap
TARGET_EXE=$(TARGET)

SRC = showmap.c
OBJ = $(SRC:.c=.o)

DEPEND_STATIC_LIB = 

CFLAGS =  

$(TARGET_EXE):$(OBJ) $(DEPEND_STATIC_LIB);$(CC) -o $@ $^ $(LDFLAGS)
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 cp -rf $(TARGET_EXE) ../../../output/bin/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_EXE)
