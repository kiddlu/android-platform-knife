CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

TARGET = libsepol
TARGET_STATIC_LIB = $(TARGET).a

SRC = \
	src/assertion.c \
	src/avrule_block.c \
	src/avtab.c \
	src/boolean_record.c \
	src/booleans.c \
	src/conditional.c \
	src/constraint.c \
	src/context.c \
	src/context_record.c \
	src/debug.c \
	src/ebitmap.c \
	src/expand.c \
	src/genbools.c \
	src/genusers.c \
	src/handle.c \
	src/hashtab.c \
	src/hierarchy.c \
	src/iface_record.c \
	src/interfaces.c \
	src/link.c \
	src/mls.c \
	src/module.c \
	src/node_record.c \
	src/nodes.c \
	src/polcaps.c \
	src/policydb.c \
	src/policydb_convert.c \
	src/policydb_public.c \
	src/port_record.c \
	src/ports.c \
	src/roles.c \
	src/services.c \
	src/sidtab.c \
	src/symtab.c \
	src/user_record.c \
	src/users.c \
	src/util.c \
	src/write.c

OBJ = $(SRC:.c=.o)

CFLAGS = -Wall -W -Wundef -Wshadow -Wmissing-noreturn -Wmissing-format-attribute
CFLAGS += -I./include -I./src

$(TARGET_STATIC_LIB):$(OBJ);$(AR) -r $(TARGET_STATIC_LIB) $(OBJ)
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 mv $(TARGET_STATIC_LIB) ../../output/lib/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_STATIC_LIB)
