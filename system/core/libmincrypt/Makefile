CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

TARGET=libmincrypt
OBJ = dsa_sig.o p256.o p256_ec.o p256_ecdsa.o rsa.o sha.o sha256.o
CFLAGS = -Wall -Werror
TARGET_STATIC_LIB = $(TARGET).a
CFLAGS += -I../include
$(TARGET_STATIC_LIB):$(OBJ);$(AR) -r $(TARGET_STATIC_LIB) $(OBJ)
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 cp -rf $(TARGET_STATIC_LIB) ../../../output/lib/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_STATIC_LIB)


