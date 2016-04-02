.PHONY:all
all:
	make -f Makefile.sepolicy-analyze
	make -f Makefile.check_seapp
	make -f Makefile.checkfc
	make -f Makefile.sepolicy-check

.PHONY:install
install:
	make -f Makefile.sepolicy-analyze install
	make -f Makefile.check_seapp install
	make -f Makefile.checkfc install
	make -f Makefile.sepolicy-check install

.PHONY:clean
clean:
	make -f Makefile.sepolicy-analyze clean
	make -f Makefile.check_seapp clean
	make -f Makefile.checkfc clean
	make -f Makefile.sepolicy-check clean
