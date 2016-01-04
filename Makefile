.PHONY:all
all:
	cd zlib/ && make && make install && make clean
	cd libmincrypt/ && make && make install && make clean
	cd libsparse/ && make && make install && make clean
	cd mkbootimg/ && make && make install && make clean
	cd cpio/ && make && make install && make clean
	cd prebuild/ && make install

.PHONY:clean
clean:
	cd zlib/ && make clean
	cd libmincrypt/ && make clean
	cd libsparse/ && make clean
	cd mkbootimg/ && make clean
	cd cpio/ && make clean
