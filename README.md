##Android Host Tools Standalone

#### How to use?

	cd android-host-tools-standalone/
	make CROSS_COMPILE=$(if-you-need)
	source setenv.sh

####Note:
	Linux   work well
	MacOS   not test
	Cygwin  work well
	MinGW   some bugs in mkbootfs(packramdisk.sh, symbol link issue)

#####TODO
	fix bugs on MinGW if possible
	