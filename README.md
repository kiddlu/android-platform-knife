##Android Host Knife

#### How to build?

	cd android-host-knife/
	make CROSS_COMPILE=$(if-you-need)
	source setenv.sh

####Note:
	Linux   work well
	MacOS   not test
	Cygwin  work well
	MinGW   bug in mkbootfs(packramdisk.sh, symbol link issue)

#####TODO
	fix bug on MinGW if possible
	