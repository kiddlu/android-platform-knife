#makefile for libmincrypt on Windows

CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar
STATIC_LIB = libmincrypt.a
OBJ = dsa_sig.o p256.o p256_ec.o p256_ecdsa.o rsa.o sha.o sha256.o
CFLAGS += -I../include

$(STATIC_LIB):$(OBJ);$(AR) -r  $(STATIC_LIB) $(OBJ)
%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

clean:
	 rm -f $(OBJ) $(STATIC_LIB)
