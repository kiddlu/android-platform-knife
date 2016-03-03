.PHONY:all
all:
	make -f Makefile.zlib
	make -f Makefile.libunz
	make -f Makefile.minigzip

.PHONY:install
install:
	make -f Makefile.zlib install
	make -f Makefile.libunz install
	make -f Makefile.minigzip install
	
.PHONY:clean
clean:
	make -f Makefile.zlib clean
	make -f Makefile.libunz clean
	make -f Makefile.minigzip clean
