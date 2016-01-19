
UNAME = $(shell uname -o)
GCCVERSION = $(shell $(CROSS_COMPILE)gcc -v)
.PHONY:all
all:
	@echo "Building for $(UNAME)"
	@echo "$(GCCVERSION)"
	cd external/zlib/ && make && make install && make clean
	cd system/core/libmincrypt/ && make && make install && make clean
	cd system/core/libsparse/ && make && make install && make clean
	cd system/core/mkbootimg/ && make && make install && make clean
	cd system/core/cpio/ && make && make install && make clean
	cd prebuild/ && make install

.PHONY:clean
clean:
	cd external/zlib/ && make clean
	cd system/core/libmincrypt/ && make clean
	cd system/core/libsparse/ && make clean
	cd system/core/mkbootimg/ && make clean
	cd system/core/cpio/ && make clean
