CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

TARGET=libmincrypt
TARGET_STATIC_LIB = $(TARGET).a

SRC = dsa_sig.c p256.c p256_ec.c p256_ecdsa.c rsa.c sha.c sha256.c
OBJ = $(SRC:.c=.o)

CFLAGS = -Wall -Werror
CFLAGS += -I../include

$(TARGET_STATIC_LIB):$(OBJ);$(AR) -r $(TARGET_STATIC_LIB) $(OBJ)
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 cp -rf $(TARGET_STATIC_LIB) ../../../output/lib/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_STATIC_LIB)


