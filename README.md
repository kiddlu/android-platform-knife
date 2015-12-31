##Android Host Binary Standalone( Linux / Darwin / Cygwin / Windows )

#### If you don't need CROSS_COMPILE, just ignore. examples:

####simg2img
	1.build zlib
	cd zlib/
	make CROSS_COMPILE=i686-pc-mingw32-
	make install

	2.build simg2img
	cd libsparse/
	make CROSS_COMPILE=i686-pc-mingw32-
	make install

####unpackbootimage & mkbootimg
	1.build libmincrypt
	cd libmincrypt/
	make CROSS_COMPILE=i686-pc-mingw32-
	make install

	2.build unpackbootimage & mkbootimg
	cd mkbootimg
	make CROSS_COMPILE=i686-pc-mingw32-
	make install