CFLAGS=-Wall -D LINUXPC

#Some Linux distros require added libraries
#LIBS = -lrt -lpthread 
MAIN = ks

OBJECTS=kickstart.o	  \
	comm.o			        \
	sahara_protocol.o   \
	kickstart_log.o			\
  kickstart_utils.o		\
  external_utils.o

all: kickstart

kickstart: $(OBJECTS)
clean:
	rm $(OBJECTS) ks
