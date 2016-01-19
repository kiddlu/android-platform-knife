##Android Host Tools Standalone

#### How to use?

	cd android-tools-binary-standalone/
	make CROSS_COMPILE=$(if-you-need)
	source setenv.sh

####Note:
	Linux   work well
	MacOS   not test
	Cygwin  some bugs in simg2img,img2simg....
	MinGW   some bugs in mkbootfs(packramdisk.sh)

#####TODO
	fix bugs on Cygwin
	fix bugs on MinGW if possible