.PHONY:all
all:
	cd zlib/ && make
	cd libmincrypt/ && make
	cd libsparse/ && make
	cd mkbootimg/ && make
	cd cpio/ && make

.PHONY:install
install:
	cd zlib/ && make install
	cd libmincrypt/ && make install
	cd libsparse/ && make install
	cd mkbootimg/ && make install
	cd cpio/ && make install
	cd prebuild/ && make install

.PHONY:clean
clean:
	cd zlib/ && make clean
	cd libmincrypt/ && make clean
	cd libsparse/ && make clean
	cd mkbootimg/ && make clean
	cd cpio/ && make clean
