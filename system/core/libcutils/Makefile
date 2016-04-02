CC = $(CROSS_COMPILE)gcc
AR = $(CROSS_COMPILE)ar

COMPILER = $(shell $(CC) -dM -E -  < /dev/null | grep _WIN32 > /dev/null && echo "mingw"  || echo "none")
ifeq ($(COMPILER), none)
COMPILER = linux
endif


TARGET=libcutils
TARGET_STATIC_LIB = $(TARGET).a

SRC = \
	hashmap.c \
	native_handle.c \
	config_utils.c \
	atomic.c \
	cpu_info.c \
	load_file.c \
	open_memstream.c \
	strdup16to8.c \
	strdup8to16.c \
	record_stream.c \
	process_name.c \
	threads.c \
	sched_policy.c \
	iosched_policy.c \
	str_parms.c \

OBJ = $(SRC:.c=.o)

DEPEND_STATIC_LIB = ../../../output/lib/liblog.a

CFLAGS = -DANDROID_SMP=0
CFLAGS += -I../include

ifeq ($(COMPILER), linux)
SRC +=  \
        fs.c \
        multiuser.c \
        socket_inaddr_any_server.c \
        socket_local_client.c \
        socket_local_server.c \
        socket_loopback_client.c \
        socket_loopback_server.c \
        socket_network_client.c \
        sockets.c \
        ashmem-host.c dlmalloc_stubs.c

CFLAGS += -DHAVE_PTHREADS
endif

ifeq ($(COMPILER), mingw)
CFLAGS += -DUSE_MINGW -DHAVE_WIN32_THREADS -DHAVE_WINSOCK
endif

$(TARGET_STATIC_LIB):$(OBJ) $(DEPEND_STATIC_LIB);$(AR) -r $(TARGET_STATIC_LIB) $(OBJ)
$(filter %.o,$(OBJ)):%.o:%.c;$(CC) $(CFLAGS) -c -o $@ $<

.PHONY:install
install:
	 cp -rf $(TARGET_STATIC_LIB) ../../../output/lib/

.PHONY:clean
clean:
	 rm -f $(OBJ) $(TARGET_STATIC_LIB) *.o
